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
  - `worldAtlas()`：返回只读 `WorldAtlas` 视图。
  - `worldVersion()`：当前世界版本号（随拓扑或 Tilemap 变更递增）。
- **IRuntimeCommands（命令面）**
  - 世界：`generateWorld(MapConfig root)`、`loadWorld(path)`、`resetWorld(seed, overrides)`。
  - 实体：`spawnAgent(params)`、`despawn(entityId)`、`applyTrait(entityId, traitId)`。
  - 资源与事件：`inject(Event e)`、`requestResource(type, amount, preferredNode)` 等。
  - 命令执行均在模拟线程排队；返回 `CommandId`，结果异步写入 Telemetry/日志。仅对快速操作可选同步 `Result`。

**并发约束：**
- Engine 仅在模拟线程运行，所有状态修改必须在此线程完成。
- 前端线程通过 `SimulationSnapshot` / `WorldAtlas` 获取数据，不得直接访问 ECS/Registry。
- `SimulationSnapshot` 是双缓冲结构，持有 `TickTelemetry` 与捕获时间戳；Atlas 是不可变结构体。两者携带版本号避免竞争条件。

## SimulationSnapshot 契约
- `version:uint64`：快照递增序列号（模拟线程每次写入时自增）。
- `capturedAt:steady_clock::time_point`：生成快照时的单调时钟，供 UI 估算停顿或插值。
- `telemetry:TickTelemetry`：与旧协议一致的遥测负载；详见下文。

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
- Tilemap 支持 JSON (`.tmj`) 与二进制 (`.tmb`)；二进制格式由 Header + Blocks 组成（详见 `sandbox_gui_tilemap_rendering.md`）。
- `tilemaps[].insideView.format` 标示当前文件类型，前端按需选择解析器。
- 优先级：中（在现行流程稳定后排期完成）。

## 迭代与验证
- 每次 API 调整需更新文档与 `schema_version`，并补充集成测试（无头跑 N 步验证新字段）。
- GUI 需在启动时校验 `schema_version`，不兼容时弹出提示并拒绝运行，以免误用旧协议。
