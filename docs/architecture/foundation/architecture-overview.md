# GenesisEngine · 核心架构总览（v3）

> 面向引擎内核与运行时的贡献者；本文描述 v3 重构后的主干模块、分层职责与数据流。设计原则继承自 [`meta/vision.md`](../meta/vision.md)，命名空间统一方案见提案 [`proposals/meta/namespace-strategy.md`](../proposals/meta/namespace-strategy.md)。

## 1. 分层视图

```
┌────────────────────────────────────────────┐
│ Interface & Experience ── RuntimeBridge ／ │
│ Sandbox GUI ／ Game ／ 自动化脚本           │
└─────────────▲──────────────────────────────┘
              │ 只读快照 + 命令 API
┌─────────────┴──────────────────────────────┐
│ Runtime Services ── Runtime ／ Snapshot ／  │
│ Command 队列 ／ 世界存取                     │
└─────────────▲──────────────────────────────┘
              │ Tick 调度／领域接口
┌─────────────┴──────────────────────────────┐
│ Simulation Kernel ── SimulationContext ／   │
│ Scheduler ／ Agents ／ World Systems        │
└─────────────▲──────────────────────────────┘
              │ 基础工具／只读数据源
┌─────────────┴──────────────────────────────┐
│ Foundation ── EventBus ／ Telemetry ／      │
│ WorldDatabase ／ Diagnostics                │
└────────────────────────────────────────────┘
```

## 2. Simulation Kernel

### 2.1 SimulationContext
- 负责持有 `entt::registry` 与领域系统实例，不再对外暴露原始 ECS。
- 管理世界级资源：
  - `world::system::ResourceSystem`——基于世界数据库的交互点生成库存实体，驱动再生与事件派发。
  - `agents::ActionExecutor`——依赖资源系统执行移动/消耗任务。
- 世界系统现由独立静态库 `Genesis::World` 提供，实现与 Simulation 解耦。
- 与 `Scheduler` 协同，按帧调用 Need → Action → Movement → World 系统。
- 提供只读采样接口 `collectResourceSnapshots`，供遥测聚合使用。

### 2.2 Scheduler
- 取代旧的“Movement + Resource”顺序，改为阶段化调度：
  1. `NeedSystem::update`：评估需求强度并刷新状态样本。
  2. `NeedSatisfier::update`：结合 `ResourceSystem` 与 `ActionExecutor` 推送动作队列。
  3. `ActionExecutor::update`：对 Agent 应用移动/消费请求。
  4. `Movement2DSystem::update`：执行直线移动与跨图瞬移。
  5. `ResourceSystem::tick`：按步再生资源并触发库存事件。
- 所有指针均为弱依赖，可按需裁剪或扩展阶段。

### 2.3 Engine
- 作为 SimulationContext 的拥有者，负责：
  - `SimulationClock` 推进与 Step 循环。
  - 快照回调、Telemetry 管线以及 Demo Agent 注入。
  - 世界装载：`loadWorldFromFile` 通过 `WorldDatabaseLoader` 生成只读数据库，并交给 SimulationContext 重建资源系统。
- 暴露受控命令接口（已移除 `registry()` 访问）：
  - `createAgent2D / setAgentMovementIntent / stopAgentMovement / teleportAgent / deleteAgent`
  - `consumeResource`（内部映射到 `ResourceSystem::consume`）
  - `queryAgentLocation`（运行时在不暴露 ECS 的情况下查询位置）

## 3. Runtime Services

### 3.1 Runtime
- 基于 `SimulationService` Facade（默认 `EngineSimulationService`）维护：
  - 命令队列（`RuntimeEvent`）与顺序执行。
  - `SimulationSnapshotBuffer` 双缓冲快照及差分计算。
  - 世界载入、保存流程（暂停工作线程、重建 Atlas）。
- 对界面层暴露的 Facade：
  - 快照读取：`latestSnapshot()` / `latestSnapshotDiff()`
  - 世界生命周期：`loadWorldFromFile` / `saveWorldToFile`
  - Agent 与资源命令：复用仿真服务提供的受控接口
  - 查询：`worldDatabase()`、`agentLocation()`、`agentExists()`
- 所有命令均在模拟线程执行，失败信息通过 `RuntimeEventReport` 回传。
- 当 `RuntimeEvent::handler` 需要访问底层 Engine 时，仅在运行时选择 `EngineSimulationService` 时可用；否则必须改写为 `runtimeHandler`。

### 3.2 Telemetry
- `TelemetryCollector::collect` 接收资源快照 DTO，与 Agent 位置组合成 `telemetry::TickTelemetry`。
- Engine 维护循环缓冲 `TelemetryBuffer` 并按固定步长输出概览日志。

## 4. Interface Layer（以 Sandbox GUI 为例）
- `RuntimeBridge` 仅访问 Runtime Facade：
  - Atlas 构建直接读取 `WorldDatabase`，不再触碰 Engine。
  - 命令脚本（create/move/delete/teleport/consume）通过 Runtime 受控 API 完成。
  - 背景线程消费快照并维护命令状态机，UI 线程只处理 DTO。
- GUI 扩展新的命令类型时只需新增脚本描述与 Runtime 调用，无需了解仿真实现。

## 5. 数据流与并发
- **模拟线程**：Runtime 的工作线程调用 `Engine::step`，拥有唯一写权限。
- **前端线程**：读取快照、Atlas 与事件报告，所有共享状态均受互斥量保护。
- **事件执行**：在模拟线程处理，成功/失败均写回 `RuntimeEventReport` 并通过快照广播。
- **遥测数据**：每 Tick 采集 Agent/Resource 状态，携带版本号与时间戳，供 UI/自动化差异比较。

## 6. 世界装载流程
1. Runtime 接收 `loadWorldFromFile` 命令并暂停工作线程。
2. `WorldDatabaseLoader` 解析 `world.json` 与 `map_*.json`，生成共享数据库。
3. Engine `destroyAllAgents()` 清理旧实体，SimulationContext `reset()` 释放旧资源系统。
4. SimulationContext 使用新数据库重建 `ResourceSystem` 与 `ActionExecutor`，将资源锚点注入 registry。
5. Engine 无世界内容时注入 Demo Agent，随后 Resume，快照版本递增。

## 7. 扩展指引
- 新增领域系统时，为 `Scheduler` 提供注册接口并定义执行阶段。
- 对外暴露的新命令应先在 Engine 添加受控方法，再由 Runtime 包装事件，最后在界面层调用。
- Telemetry 扩展遵循 DTO 原则，避免泄露 ECS。
- 界面层禁止直接引用 `entt::registry`；若需查询状态，先为 Runtime 增加 Facade 方法。

> 旧版 `ResourceSystem2D` 与 `Engine::registry()` 已移除，相关业务需要迁移至上述 Facade。
