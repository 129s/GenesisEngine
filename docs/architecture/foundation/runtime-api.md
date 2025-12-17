# Runtime API（v2 · Map 图 + 直线移动）

目标：为前端与工具提供稳定、线程安全、可演进的接口。API 分为控制面、查询面、命令面三个维度；通过 Telemetry/WorldAtlas 暴露只读数据。运行时语义采用“Map 内直线移动 + Map 图（有向）跨图拼接”。

## API 切面与并发模型
- IRuntimeControl（控制面）
  - `setPaused(bool)` / `paused()`
  - `requestStep(uint64_t steps)`（暂停时逐步推进）
  - `setSpeedMultiplier(double)`（调整模拟时钟倍率）
  - 生命周期：`start()` / `stop()`（通常由宿主封装）
- IRuntimeQuery（查询面）
  - `latestSnapshot()`：返回 `SimulationSnapshot` 只读视图（双缓冲最新帧，含 `version`/`capturedAt`/`TickTelemetry`）
  - `latestSnapshotDiff()`：返回与上一帧对比的差分（无上一帧视为全量）
  - `worldAtlas()`：返回只读 `WorldAtlas` 视图（包含 `maps`/`mapEdges` 与每图的 `scenes/interactions/portals[/tilemap]`）
  - `worldVersion()`：世界版本号（随 Map/Scene/Interaction/Tilemap 元变更递增）
- IRuntimeCommands（命令面，JSON 描述）
  - 世界（v2）：
    - `world.db.load { folder }`：从目录加载 `world.json + map_{id}.json`
    - `world.db.save { folder }`：保存当前 DB 到目录
    - `world.db.reload { folder }`：重载并刷新 Atlas
    - `world.db.generate { configPath, seed?, outputFolder? }`：根据 worldgen 配置生成世界、落盘到目录并自动加载
    - 兼容别名（已弃用）：`world.load { path }` / `world.save { path }` / `world.reload { path }`（仅保留过渡期；v2 推荐统一使用 `world.db.*`）
    - 兼容别名（已弃用）：`world.generate { configPath, seed?, outputFolder? }`（推荐改用 `world.db.generate`）
  - 实体（v2 2D）：
    - `agent.create2d { mapId, x, y, move?{ mapId,x,y,speed } }`
    - `agent.move2d   { entityId, mapId, x, y, speed }`
    - `agent.stop2d   { entityId }`
    - `agent.teleport2d { entityId, mapId, x, y }`
    - `agent.delete2d { entityId }`
  - 资源：
    - `resource.consume { interactionId, amount }`
  - 事件执行：统一通过 `enqueueEvent(RuntimeEvent)` 串行执行；返回 `eventId`，结果在 `RuntimeEventReport` 中体现。

> 实现提示：当前仓库已在 `Genesis::Runtime::Runtime` 中提供 `worldAtlas()/worldVersion()` 与 `enqueueCommandFromJson(...)` 的基础实现，可作为 Headless 工具与测试入口。

Headless 工具：仓库提供 `genesis-runtime-cli`（见 `docs/guides/headless_runtime_cli.md`），用于执行 JSON 脚本并回收事件结果。

并发约束：
- 模拟线程是唯一写入者；前端线程只读 `SimulationSnapshot` / `WorldAtlas`
- `SimulationSnapshot` 双缓冲；Atlas 不可变；二者携带版本号避免竞争

## SimulationSnapshot 契约（最小集）
- `version:uint64`、`capturedAt:steady_clock::time_point`
- `telemetry:TickTelemetry`
- `events:RuntimeEventReport[]`（命令/标记的执行记录）

## SimulationSnapshotDiff 契约
- `baseVersion:uint64?` / `targetVersion:uint64`、`baseStep?:uint64` / `targetStep:uint64`
- `capturedAt:steady_clock::time_point`、`executedEvents:RuntimeEventReport[]`
- 变更集合（按需扩展）：
  - `resourceChanges[]` / `needChanges[]` / `plannerChanges[]` / `actionChanges[]` / `agentChanges[]` / `movementChanges[]`

## TickTelemetry 契约（v6）
最小字段集合，按需扩展；变更需提升 `schema_version` 并记录（GUI 以 schema 做兼容）。

完整字段字典与变更记录见：`docs/architecture/foundation/telemetry-schema.md`。
- `schema_version:uint32`
- `step:uint64`、`stepSeconds:float`
- `agents[]`：
  - `entityId:uint32`
  - `name:string`
  - `mapId:uint32`
  - `position:{ x:float, y:float }`（地图内世界坐标；直线移动语义）
- `movements[]`（可选）
- `resources[]`：`{ interactionId:uint32, mapId:uint32, name:string, type:enum, current:uint32, capacity:uint32, consumed:uint32, produced:uint32, decayed:uint32 }`
- `workshopAttempts[]`：工坊生产尝试与失败原因（用于解释“为何没产出/卡在哪”）。
- `resourceAttempts[]`：资源获取尝试与失败原因（Consume/Take；用于解释“为何拿不到/是否规划了补链”）。
- `needs[]`、`plannerDecisions[]`、`actions[]`：按需扩展；可作为 Headless/Soak 分析输入

## WorldAtlas 契约（v2）
Atlas 描述世界静态结构（Map 图）与每图可视元数据；只读、版本化。
- `world_version:uint32`、`schema_version:uint32`
- `maps[]: { id:uint32, name:string, meta?:object }`
- `mapEdges[]: { from:uint32, to:uint32, bidirectional?:bool, cost?:number, rules?:object }`
- `perMap[]: { mapId:uint32, scenes:Scene[], interactions:Interaction[], portals:Portal[], tilemap?:TilemapMeta }`
  - `Scene{ id:uint32, parent?:uint32, name:string, origin?:[int,int], transform?:object, meta?:object }`
  - `Interaction{ id:uint32, sceneId:uint32, kind:string, coord_local:[int,int], coord_global?:[int,int], meta?:object, capacity?:uint32, regen?:uint32 }`
  - `Portal{ interactionId:uint32, channelId?:string, oneWay?:bool, teleportCost?:number }`
  - `TilemapMeta{ width:int, height:int, tileW:int, tileH:int }`

## 事件模型（命令队列）
`RuntimeEvent`/`RuntimeEventReport`：
- 字段：`id`、`kind:Command|Marker`、`label`、`payloadJson?`、`enqueuedAt`、`executedAt`、`success`、`message`
- 处理：`Runtime::enqueueEvent` 将事件入队；`step/run` 前串行执行；报告写入下一帧快照与 diff

### RuntimeBridge 命令描述（JSON）
- 顶层：`name?:string`，`commands:CommandDescriptor[]`
- `CommandDescriptor`：`action:string`（如 `world.db.load|agent.create2d|resource.consume`）、`label?`、`waitForSuccess?:bool`、其余字段入 `payloadJson`
- v2 Runtime 已提供 Headless 的 `enqueueCommandSequenceFromJson(...)` 实现（支持 `waitForSuccess` 串联）；GUI 侧仅需提供脚本加载/显示即可

## 兼容性与演进
- 兼容旧协议：保留 `latestSnapshot()`/`latestSnapshotDiff()` 的接口形式；字段迁移采用新增/弃用并行期
- 版本化：`schema_version` 与 `world_version` 双轨控制；前端按版本刷新缓存与字段解析
- 运行时不读取 Tilemap/碰撞；世界表达详见 `world-model.md`

版本策略与门禁：见 `docs/architecture/foundation/schema-versioning.md`。
