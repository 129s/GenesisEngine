# Runtime 接口设计（建议草案）

目标：提供稳定、线程安全、可演进的 Runtime 接口，前端（CLI/GUI）通过它做控制、查询与事件注入。

## 模块与并发模型
- IRuntimeControl（控制面）
  - `setPaused(bool)` / `paused()`
  - `requestStep(uint64_t n)`（暂停时逐步）
  - `setSpeedMultiplier(double)`（运行时节流）
  - 生命周期：`start()`/`stop()`（或由宿主封装）
- IRuntimeQuery（查询面）
  - `latestSnapshot()`：返回 `TickTelemetry` 的拷贝（环形缓冲尾部）
  - `worldAtlas()`：返回只读 `WorldAtlas`（随世界版本变更重建）
  - `worldVersion()`：单调递增的版本号（Atlas/数据兼容检测）
- IRuntimeCommands（命令面）
  - 世界：`generateWorld(NoiseParams)`、`loadWorld(path)`、`resetWorld(seed/opts)`
  - 实体：`spawnAgent(params)`、`despawn(entityId)`、`setIntent(entityId, MovementIntent)`
  - 资源：`requestResource(type, amount, preferredLocation)`（已有）
  - 事件：`inject(Event e)`（扩展型）
  - 返回：立即 `CommandId`，异步通过 Telemetry/Event 流回执；或同步返回 `Result`（仅限快速操作）

并发与安全：
- Runtime/Engine 在模拟线程推进；UI/CLI 仅在读侧消费 `Telemetry/Atlas`，不直接访问 ECS/Registry。
- Telemetry 为“数据拷贝”，Atlas 为“只读结构”，以版本号避免竞态。

## 数据形状（最小集 · 本次重构契约）
- TickTelemetry（UI 仅读）
  - 基本：`step:uint64`、`step_seconds:float`（例如 0.5）
  - 代理：`agents[{ entityId:uint32, name:string, location:LocationId }]`
  - 行动：`actions[{ entityId, currentAction, queueLength, target, ... }]`
  - 需求：`needs[{ entityId, needName, value, critical }]`
  - 资源：`resources[{ location:LocationId, name, type, current, capacity }]`
  - 行进进度：`movement_progress[{ entityId, from:LocationId, to:LocationId, t01:float }]`（0..1，用于 Map 边上插值）
- WorldAtlas（UI 仅读）
  - 节点：`nodes[{ id, parent, kind, name, coord_global:[int,int] }]`
  - 边：`edges[{ from, to, bidirectional, anchors:{ at_from:[int,int], at_to:[int,int] }, polyline?:[[int,int],...] }]`
  - 资源点：`spawns[{ resource:{ name,type,location,capacity,ratePerStep }, position_global:[int,int] }]`
  - 场景：`tilemaps[{ nodeId, width:int, height:int, tileSizePx:int, layersMeta, portals:[{ to:LocationId, anchor:[int,int] }] }]`
  - 查找/版本：`nodeLookup{ id->coord_global }`、`world_version:uint32`、`schema_version:uint32`

## 事件与版本化
- 命令执行改变世界（拓扑/资源/实体），Runtime 提升 `world_version`，重建 Atlas，并在下一帧快照反映变化。
- Telemetry/Atlas 含 `schema_version` 字段，保证前后兼容；前端可按版本降级或拒绝不兼容数据。

## 错误与诊断
- 控制/命令返回 `Result{ok, error, details}`；重大错误写入日志（spdlog），并在 Telemetry `diagnostics` 中暴露摘要。
- 性能指标：可选在 Telemetry 增加 `timing{system_ms...}`，辅助前端显示 HUD。
