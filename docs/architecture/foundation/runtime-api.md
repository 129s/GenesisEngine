# Runtime API 设计

目标：为前端与工具提供稳定、线程安全、可演进的接口。API 分为控制面、查询面、命令面三个维度，并通过 Telemetry/WorldAtlas 向外暴露数据。

## API 切面与并发模型
- **IRuntimeControl（控制面）**
  - `setPaused(bool)` / `paused()`
  - `requestStep(uint64_t steps)`：暂停状态下逐步推进。
  - `setSpeedMultiplier(double)`：调整模拟时钟倍率。
  - 生命周期：`start()` / `stop()`（通常由宿主封装）。
- **IRuntimeQuery（查询面）**
  - `latestSnapshot()`：返回 `SimulationSnapshot` 只读视图（双缓冲最新帧，包含 `version` / `capturedAt` / `TickTelemetry`）。
  - `latestSnapshotDiff()`：返回 `SimulationSnapshotDiff`，与上一帧比对变化（若无上一帧则视为全量新增）。
  - `worldAtlas()`：返回只读 `WorldAtlas` 视图。
  - `worldVersion()`：当前世界版本号（随拓扑或 Tilemap 变更递增）。
- **IRuntimeCommands（命令面）**
  - 世界：`generateWorld(MapConfig root)`、`loadWorld(path)`、`resetWorld(seed, overrides)`。
  - 实体：`spawnAgent(params)`、`despawn(entityId)`、`applyTrait(entityId, traitId)`。
  - 资源与事件：`requestResource(type, amount, preferredNode)`、`enqueueEvent(RuntimeEvent)`（命令队列/事件注入）。
  - `Runtime::enqueueEvent` 返回自增的 `eventId`，用于前端跟踪命令状态；事件始终在模拟线程串行执行，结果异步写入 Telemetry/日志（`RuntimeEventReport`）。仅对快速操作可选同步 `Result`。

**并发约束：**
- Engine 仅在模拟线程运行，所有状态修改必须在此线程完成。
- 前端线程通过 `SimulationSnapshot` / `WorldAtlas` 获取数据，不得直接访问 ECS/Registry。
- `SimulationSnapshot` 是双缓冲结构，持有 `TickTelemetry` 与捕获时间戳；Atlas 是不可变结构体。两者携带版本号避免竞争条件。
- 事件注入通过 `Runtime::enqueueEvent` 排队，`Runtime::step/run` 在每次推进前串行执行命令并将结果附带到下一帧 `SimulationSnapshot.events`。

## SimulationSnapshot 契约
- `version:uint64`：快照递增序列号（模拟线程每次写入时自增）。
- `capturedAt:steady_clock::time_point`：生成快照时的单调时钟，供 UI 估算停顿或插值。
- `telemetry:TickTelemetry`：与旧协议一致的遥测负载；详见下文。
- `events:RuntimeEventReport[]`：本帧执行的命令/标记；包含 `id`、`label`、`kind`、`payloadJson?`、`enqueuedAt`、`executedAt`、`success`、`message`。

## SimulationSnapshotDiff 契约
- `baseVersion:uint64` / `targetVersion:uint64`：对比的起止快照版本；首帧无 `baseVersion` 时置 0 并 `hasBase=false`。
- `baseStep?:uint64` / `targetStep:uint64`：对应模拟 step，便于 UI 做时间线标记。
- `capturedAt:steady_clock::time_point`：目标帧时间戳。
- `resourceChanges[]` / `needChanges[]` / `plannerChanges[]` / `actionChanges[]` / `agentChanges[]` / `movementChanges[]`：
  - 元素类型为 `SnapshotChange<T>`，字段：`kind:Added|Removed|Modified`、`before?:T`、`after?:T`。
  - `kind=Added` 仅提供 `after`，`kind=Removed` 仅提供 `before`，`Modified` 同时提供前后差异。
- `executedEvents:RuntimeEventReport[]`：与目标帧一致的事件日志（便于重建时间线或断言命令执行结果）。
- `empty():bool`：若所有变更集合为空且无事件，返回 `true`，可用于快速跳过无改动帧。
- `latestSnapshotDiff()` 默认与上一帧对比；调用方可缓存版本号用于断言期望的变化是否出现。

## TickTelemetry 契约
最小字段集合如下，可根据功能扩展。所有字段必须注明 `schema_version`。
- `step:uint64`、`step_seconds:float`
- `sim_time:{ hours:int, minutes:int, seconds:int }`（可选，将 step 映射为模拟时间）
- `agents[]`：
  - `entityId:uint32`
  - `name:string`
  - `nodeId:uint32`（当前 Anchor/Interaction/Area）
  - `mapId:string`
  - `localPosition:{ x:float, y:float }`（无局部坐标时置空，下游可退化为锚点中心）
  - `movementProgress?:{ fromNode, toNode, t01:float }`
  - `currentGoal?:{ need:string, targetNode:uint32 }`
  - `traits[]`（便于 Inspector 展示）
- `needs[]`：`{ entityId, needId, intensity:float, threshold:float, critical:bool }`
- `attributes[]`：`{ entityId, attributeId, value:float }`
- `actions[]`：`{ entityId, queueLen:int, activeAction:string, etaSteps:int }`
- `resources[]`：`{ nodeId, mapId, type, current, capacity, regenPerStep }`
- `diagnostics`：性能计数、警报、日志摘要（可选）。

扩展字段需同步更新 `schema_version` 并在文档中记录。

## WorldAtlas 契约
Atlas 描述世界静态结构与 Tilemap 元数据。字段建议如下：
- `world_version:uint32`、`schema_version:uint32`
- `nodes[]`：
  - `id:uint32`
  - `parent?:uint32`
  - `kind:string`（Region/District/Area/Anchor/Interaction/Portal…）
  - `role?:string`（例如 `anchor`, `interaction`, `portal`）
  - `name:string`
  - `mapId:string`
  - `coord_local:{ x:int, y:int }`（在 map 内的瓷砖坐标）
- `edges[]`：
  - `from:uint32`, `to:uint32`
  - `bidirectional:bool`
  - `cost:float`
  - `portalNode?:uint32`（若为 Portal 特殊节点，指向对应 nodeId）
- `portals[]`（便于直接查询）：
  - `nodeId:uint32`
  - `from:{ mapId, coord:{x:int,y:int} }`
  - `to:{ mapId, coord:{x:int,y:int} }`
  - `width:int`, `passMask:uint32`
- `tilemaps[]`：
  - `mapId:string`
  - `outsideView:{ format:string, path:string, bounds:{width:int,height:int} }`
  - `insideView:{ format:string, path:string, width:int, height:int, tileSize:int, layers:int }`
  - `children[]:{ childMapId:string, portalNode:uint32 }`
  - `binary:{ enabled:bool, preferredFormat?:string }`（标记二进制协议，可选）
- `lookup`：
  - `nodeById`、`nodesByMapId` 索引结构

当世界重建或拓扑变更时，Runtime 会：
1. 在模拟线程重建 Registry/Atlas；
2. 提升 `world_version`；
3. 下发新的 Atlas 引用；旧引用仍然有效但版本落后。

## 版本与兼容策略
- `schema_version` 采用 `MAJOR.MINOR`：
  - MAJOR 变动意味着向后不兼容（前端需升级或拒绝数据）。
  - MINOR 变动表示新增字段或可选信息，老客户端可忽略。
- `world_version` 自增，前端收到新版本后应销毁旧的 Tilemap 缓存并重新构建渲染资源。
- Telemetry/Atlas 中保留 `reserved` 字段，为未来扩展提供缓冲。

## 错误与诊断
- 所有命令返回 `Result{ status:Ok|Error|Queued, message, commandId }`。
- Runtime 在错误时记录 `spdlog` 日志，并在下一帧 `TickTelemetry.diagnostics` 中写入错误摘要或告警等级。
- 可选 `timing` 字段：`{ system:{ move:ms, planner:ms, renderPrep:ms }, ringBufferFill:int }`，供性能分析。

## 二进制协议（规划）
- Tilemap 支持 JSON (`.tmj`) 与二进制 (`.tmb`)；二进制格式由 Header + Blocks 组成（详见 [interface/sandbox/sandbox-gui-tilemap-rendering.md](../interface/sandbox/sandbox-gui-tilemap-rendering.md)）。
- `tilemaps[].insideView.format` 标示当前文件类型，前端按需选择解析器。
- 优先级：中（在现行流程稳定后排期完成）。

## 迭代与验证
- 每次 API 调整需更新文档与 `schema_version`，并补充集成测试（无头跑 N 步验证新字段）。
- GUI 需在启动时校验 `schema_version`，不兼容时弹出提示并拒绝运行，以免误用旧协议。
- `latestSnapshotDiff()` 默认与上一帧对比；调用方可缓存版本号用于断言期望的变化是否出现。

## RuntimeEvent 命令队列
- `RuntimeEvent` 字段：
  - `id:uint64`：Runtime 分配的自增序号，可用于重放或断言执行顺序。
  - `kind:Command|Marker`：区分带副作用的命令与仅记录时间线的标记。
  - `label:string`：事件描述（GUI/日志展示用），建议唯一化。
  - `payloadJson?:string`：可选的 JSON 负载，序列化 UI 参数或测试上下文。
  - `handler(std::function<void(Engine&)>)`：可选；在模拟线程执行，直接操作 `Engine`/`EventBus`。
  - `runtimeHandler(std::function<void(Runtime&)>)`：可选；当命令需要访问 Runtime 封装（如 `generateWorldFromConfig`）时使用。
  - `onComplete(std::function<void(RuntimeEventReport&)>)`：可选；命令执行后回调，可补充 `message` 或写入自定义元信息。
- `Runtime::enqueueEvent(event)`：线程安全，将事件加入队列；立即返回事件 ID，事件在下一次 `step`/`run` 前串行执行。
- `Runtime` 每次推进前依次执行所有排队事件，将执行结果写入 `RuntimeEventReport` 并附着到下一帧 `SimulationSnapshot.events` 与 `SimulationSnapshotDiff.executedEvents`。
- 事件处理中的异常会被捕获并写入 `message` 字段，同时 `success=false`；调用方可在 diff/快照中读取结果并决定是否中止。
- 建议：
  - GUI 交互：使用 `kind=Command`，在 handler 内部调用业务 API（例如生成世界、调整资源、插入测试 Agent）。
  - 自动化测试：使用 `payloadJson` 序列化断言上下文，结合 `latestSnapshotDiff` 校验。
  - Marker：无副作用时可省略 handler，仅用于在时间线插入标签（仍会返回成功事件记录）。

### RuntimeBridge 命令描述（JSON）

`RuntimeBridge` 在 GUI/工具链中提供了 JSON 命令描述与脚本加载能力，供命令队列批量执行：

- 顶层字段：
  - `name?:string`：脚本名称，便于日志标识。
  - `commands:CommandDescriptor[]`：按顺序执行的命令集合。
- `CommandDescriptor` 字段：
  - `action:string`：必填；当前支持 `world.generate` / `world.load` / `world.save`。
  - `label?:string`：命令标签，默认与 `action` 相同。
  - `waitForSuccess?:bool`：为 `true` 时，后续命令将等待该命令成功后再入队；失败会终止脚本。
  - 其余字段将进入 `payloadJson` 并在 handler 内解析（例如 `configPath`、`seed`、`path` 等）。
- `RuntimeBridge::enqueueCommandSequence(json, source)` 会解析脚本、按顺序提交命令，并在 GUI 侧维护 `pending`/`history` 列表（最多保留 128 条记录）。
- GUI “World Generation” 面板使用上述机制：用户输入路径后点击按钮即向命令队列提交 JSON 命令，结果通过状态面板和 `SimulationSnapshot.events` 返回。
  - 其中 `world.generate` 等重型操作会由 RuntimeBridge 启动异步任务：先暂停 Runtime、调用同步 API 完成生成，再恢复 Runtime，避免与模拟线程竞争。

示例脚本可参考 `data/scripts/world_cycle.json`，展示生成→加载→备份的命令链，并利用 `waitForSuccess` 串联操作。
