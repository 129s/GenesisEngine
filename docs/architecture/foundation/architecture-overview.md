# GenesisEngine · Architecture Overview

GenesisEngine 的使命是构建“可扩展、可观测、可复用”的涌现式叙事模拟核心。运行层以单线程 `Engine` 为核心，向外暴露稳定的 Runtime API，前端（CLI / GUI / Game）共享同一套快照与事件接口。本文概览分层结构、世界表达、行为循环与并发边界。

## 架构分层（自下而上）
- **Core Runtime**
  - `WorldRegistry`：管理 Scene/Interactive 节点树，维护全局/局部坐标、Portal、资源点等索引。
  - 系统集合：Movement、Needs、Planner、ActionExecutor、ResourceSystem 等纯逻辑系统，按离散 `SimulationClock` 推进。
  - TelemetryBuffer：聚合每步 `TickTelemetry`，由 Runtime 写入双缓冲 `SimulationSnapshot` 供前端无锁读取。
- **Runtime 封装**
  - 控制面：提供 `step`/`run`/`bootstrapSteps` 等入口，未来扩展命令队列与事件注入。
  - 查询面：暴露 `latestSnapshot()`（`SimulationSnapshot`）与 `WorldRegistry` 只读镜像（Telemetry / WorldAtlas）。
  - 协议：所有外部读写都走 Runtime，不允许前端直接操作 ECS。
- **前端适配**
  - CLI（遗留调试）：ASCII 视图 + 命令脚本。
  - GUI（主力）：GLFW + ImGui，消费 Scene/Interactive 数据绘制地图与面板。
  - Game（规划中）：与 GUI 共享 Runtime 契约，后续扩展演出与交互层。

## 世界表达：Scene/Interactive 双轨
- **运行层节点树**
  - `Scene`：可包含子 Scene 或 Interactive。负责声明布局策略（静态网格、算法生成等），在加载/生成时为子节点写入局部/全局整格坐标。
  - `Interactive`：叶节点，只承担交互；当前内置 `resource`（资源点）与 `portal`（场景切换/传送）。可扩展更多类型（作坊、事件触发器等）。
  - 边（Edges）：记录 Scene 节点之间的连通性与移动成本；Portal 会生成对应的双向边。
  - `WorldRegistry` 提供查找/子节点/边/资源等查询，并保证节点与坐标的一致性。
- **渲染层参考**
  - 渲染只读取运行层暴露的坐标与类型；视口裁剪后绘制可见 Scene/Interactive。
  - 当需要 Tilemap 细节时，Scene 的布局描述会给出锚点参考，供 GUI 或工具加载对应的瓦片资源。

两条轨道共享 ID 与坐标：运行层负责行为与寻路，渲染层只消费数据，不回写逻辑状态。

## 行为循环（Attribute → Need → Motive → Plan → Action）
1. **属性更新**：根据时间衰减、事件或资源消耗更新属性（health/hunger/sanity...）。
2. **需求评估**：NeedSystem 读取属性、人格（OCEAN）、Traits，计算需求强度及阈值（支持数据驱动配置）。
3. **动机/目标**：Planner（如 HungerPlanner）基于需求挑选目标 Scene/Interactive，写入 `MovementIntent` / `ActionQueue`。
4. **行动执行**：MovementSystem、ActionExecutor 驱动 Agent 按边移动、消费资源、触发 Portal；ResourceSystem 结算库存与产出。
5. **观测记录**：Telemetry 捕捉资源、需求、规划决策、行动队列、代理位置等快照，供前端渲染与诊断。

所有阶段都由 `SimulationClock` 控制，默认单线程推进，确保行为和世界状态的一致性。

## 并发与数据流
- 模拟线程：唯一可以修改世界状态的线程；命令需排队执行。
- 前端线程：只读取 `SimulationSnapshot` / WorldAtlas，禁止直接访问 ECS。GUI 若需局部寻路，只能基于只读数据独立计算。
- 版本机制：世界数据变化会更新 `world_version`，前端可按版本刷新缓存；Telemetry 携带 `schema_version` 以便协议演进。

## 相关文档
- 运行时接口：[foundation/runtime-api.md](runtime-api.md)
- 世界模型与生成：[world/world-model.md](../world/world-model.md)、[world/world-generation.md](../world/world-generation.md)
- 行为与人格建模：[agents/agent-personality-big5.md](../agents/agent-personality-big5.md)
- 渲染/调试：见 [interface/sandbox/sandbox-gui.md](../interface/sandbox/sandbox-gui.md) 及相关子文档
