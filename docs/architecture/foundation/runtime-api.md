# Runtime API 设计（对齐 Map 图 + 直线移动）

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
- IRuntimeCommands（命令面）
  - 世界：`generateWorld(config)`、`loadWorld(path)`、`saveWorld(path)`、`resetWorld(seed, overrides)`
  - 实体：`spawnAgent(params)`、`despawn(entityId)`、`applyTrait(entityId, traitId)`
  - 资源/事件：`requestResource(type, amount, interactionId?)`、`enqueueEvent(RuntimeEvent)`
  - `enqueueEvent` 返回自增 `eventId`；事件在模拟线程串行执行，结果写入 `RuntimeEventReport`

并发约束：
- 模拟线程是唯一写入者；前端线程只读 `SimulationSnapshot` / `WorldAtlas`
- `SimulationSnapshot` 双缓冲；Atlas 不可变；二者携带版本号避免竞争

## SimulationSnapshot 契约
- `version:uint64`、`capturedAt:steady_clock::time_point`
- `telemetry:TickTelemetry`
- `events:RuntimeEventReport[]`（命令/标记的执行记录）

## SimulationSnapshotDiff 契约
- `baseVersion:uint64?` / `targetVersion:uint64`、`baseStep?:uint64` / `targetStep:uint64`
- `capturedAt:steady_clock::time_point`、`executedEvents:RuntimeEventReport[]`
- 变更集合（按需扩展）：
  - `resourceChanges[]` / `needChanges[]` / `plannerChanges[]` / `actionChanges[]` / `agentChanges[]` / `movementChanges[]`

## TickTelemetry 契约
最小字段集合，按需扩展；变更需提升 `schema_version` 并记录。
- `schema_version:uint32`
- `step:uint64`、`step_seconds:float`
- `agents[]`：
  - `entityId:uint32`
  - `name:string`
  - `mapId:uint32`
  - `position:{ x:float, y:float }`（地图内世界坐标；直线移动语义）
  - `atInteractionId?:uint32`（若正位于某交互点）
  - `movement?:{ from:{ mapId:uint32, interactionId?:uint32 }, to:{ mapId:uint32, interactionId?:uint32 }, t01:float }`（跨图时 `from.mapId != to.mapId`）
  - `currentGoal?:{ need:string, target:{ mapId:uint32, interactionId?:uint32 } }`
  - `traits[]`
- `needs[]`：`{ entityId, needId, intensity:float, threshold:float, critical:bool }`
- `attributes[]`：`{ entityId, attributeId, value:float }`
- `actions[]`：`{ entityId, queueLen:int, activeAction:string, etaSteps:int }`
- `resources[]`：`{ interactionId?:uint32, mapId:uint32, type, current, capacity, regenPerStep }`
- `diagnostics`：性能计数、警报、日志摘要（可选）

## WorldAtlas 契约
Atlas 描述世界静态结构（Map 图）与每图可视元数据；只读、版本化。
- `world_version:uint32`、`schema_version:uint32`
- `maps[]: { id:uint32, name:string, meta?:object }`
- `mapEdges[]: { from:uint32, to:uint32, cost:float, rules?:object }`
- `perMap[]: { mapId:uint32, scenes:Scene[], interactions:Interaction[], portals:Portal[], tilemap?:TilemapMeta }`
  - `Scene{ id:uint32, parent?:uint32, name:string, origin?:[int,int], transform?:object, meta?:object }`
  - `Interaction{ id:uint32, sceneId:uint32, kind:string, coord_global:[int,int], meta?:object }`
  - `Portal{ interactionId:uint32, channelId?:string, oneWay?:bool }`
  - `TilemapMeta{ width:int, height:int, tileW:int, tileH:int }`

## 事件模型（命令队列）
`RuntimeEvent`/`RuntimeEventReport`：
- 字段：`id`、`kind:Command|Marker`、`label`、`payloadJson?`、`enqueuedAt`、`executedAt`、`success`、`message`
- 处理：`Runtime::enqueueEvent` 将事件入队；`step/run` 前串行执行；报告写入下一帧快照与 diff

### RuntimeBridge 命令描述（JSON）
- 顶层：`name?:string`，`commands:CommandDescriptor[]`
- `CommandDescriptor`：`action:string`（如 `world.generate|world.load|world.save`）、`label?`、`waitForSuccess?:bool`、其余字段入 `payloadJson`
- GUI 可加载脚本、维护 pending/history 列表、串联命令并在报告到达时推进序列

## 兼容性与演进
- 兼容旧协议：保留 `latestSnapshot()`/`latestSnapshotDiff()` 的接口形式；字段迁移采用新增/弃用并行期
- 版本化：`schema_version` 与 `world_version` 双轨控制；前端按版本刷新缓存与字段解析
- 运行时不读取 Tilemap/碰撞；以上语义变更已在 `world-model.md` 与 `world-representation.md` 对齐

