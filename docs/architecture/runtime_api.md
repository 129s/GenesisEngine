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

## 数据形状（最小集）
- TickTelemetry
  - `step`、`agents[{entityId, location}]`、`actions[{entityId, currentAction}]`、`needs[]`、`resources[{location, current, capacity, type, name}]`
  - 可选：`movement[{entityId, from, to, progress_01}]`（供 UI 平滑插值，避免跨线程读 ECS）
- WorldAtlas
  - `nodes[{id, parent, kind, name, position}]`、`edges[{from, to, bidirectional}]`、`spawns[{resource, position}]`、`nodeLookup{id->position}`、`extent`
  - 版本：`world_version`（当世界生成/加载变更时 +1）

## 事件与版本化
- 命令执行改变世界（拓扑/资源/实体），Runtime 提升 `world_version`，重建 Atlas，并在下一帧快照反映变化。
- Telemetry/Atlas 可带 `schema_version` 字段，保持前后兼容；前端按版本降级。

## 错误与诊断
- 控制/命令返回 `Result{ok, error, details}`；重大错误写入日志（spdlog），并在 Telemetry `diagnostics` 中暴露摘要。
- 性能指标：可选在 Telemetry 增加 `timing{system_ms...}`，辅助前端显示 HUD。

