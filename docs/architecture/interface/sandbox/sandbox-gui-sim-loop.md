# Sandbox GUI · 模拟循环与多代理观测

目标：在 Sandbox GUI 中展示完整的模拟循环，验证“需求→决策→行动”链路的可视化表现，确保长时间运行稳定；人格/Traits 属于后续 proposal。

## 当前实现摘要
- 线程/数据流
  - RuntimeBridge：后台线程推进 Runtime，支持 Pause/Step/Speed，维护环形 Telemetry 缓冲（默认 96 帧）。
  - GUI 线程：消费最新快照，驱动 Map/Scene/Telemetry/Inspector 面板。
- 世界/可视化
  - WorldAtlas：提供 `maps/mapEdges` 与每图的 `scenes/interactions/portals[/tilemap]`；MapView 绘制 Map 图，SceneView 渲染 Tile 层并高亮交互点/Portal。
  - 代理覆盖层：按 Telemetry 中的 `mapId + position(x,y)` 绘制，支持名称标签、状态着色。
- 世界数据
  - Demo 世界基于 MapConfig 生成，包含若干 Portal/交互点，用于验证“移动→交互→Telemetry 可观测”的闭环。

## 主要缺口
- 仅加载单个 Demo Agent，尚未验证多人差异化行为。
- 决策层目前是固定权重规则，尚未接入人格/Traits/数据驱动配置。
- 运动采用直线语义（Map 内），不做 Tile 层路径；Inspector 功能待完善。

## 目标拆解
1. **模拟循环可视化**
   - 播放控制：Pause/Resume/Step/Speed（已具备）。
   - Telemetry 面板：展示需求强度、规划/行动摘要（系统耗时类曲线若需要，可由前端自行采样或在 Telemetry 侧新增字段）。
   - Map/Scene 视图同步：Portal/锚点高亮，世界版本切换时正确刷新。
   - 稳定性：连续运行 ≥1h 无崩溃、内存泄漏、渲染错误。
2. **多代理差异化（Proposal）**
   - 引入 `AgentPersonalityBig5` + `AgentTraits` 组件（proposal），驱动差异化决策。
   - Engine 启动生成多名 Demo 代理（画像/命名），并把“身份信息”暴露给 Telemetry。
   - 让差异化发生在既有决策链路中（NeedSatisfier/Planner 打分），而不是硬编码模板集合。
   - GUI 展示差异：Map/Scene 标签、Inspector 目标/行动摘要、Telemetry 需求曲线。
3. **可视化细节（位置插值）**
   - 仅进行直线位置插值；跨图时利用 `movement{from,to,t01}` 做过渡提示。

## 人格链路回顾
当前已落地的链路：Need（数值随时间上升）→ 选点/决策 → 行动队列 → 移动/交互。规范见：
- `docs/architecture/agents/agent-model-v0.md`
人格/Traits/属性映射属于 proposal，见：
- `docs/architecture/proposals/agents/agent-personality-big5.md`

## 系统接入点
- **Needs**：`NeedSystem` 每步更新需求值；`NeedSatisfier` 做固定规则打分选点（当前未接入 Big5/Traits）。
- **行动与执行**：`ActionExecutor` 执行 Move/Consume/Take/Produce（含工坊生产作业时间、并行槽位与临界需求抢占中断）。
- **移动**：`Movement2DSystem`（Map 内直线移动语义）；可选后续在 Telemetry 暴露 `movement_progress{from,to,t01}` 供插值。
- **资源/工坊**：`ResourceSystem` 维护 Source regen/decay 与工坊库存；`ProduceResource` 解析配方并产出。
- **Engine 初始化**：当前 `spawnDemoAgentsIfEmpty()` 仅生成 1 名 Demo Agent（用于链路验证）。

## Telemetry / GUI 增量
- **当前 TickTelemetry（v5）**：`agents[]/needs[]/plannerDecisions[]/actions[]/resources[]/workshopAttempts[]/resourceAttempts[]/movements[]`（字段字典见 `docs/architecture/foundation/telemetry-schema.md`）。
- **已具备的 Agent 字段**：`name`、`mapId`、`position(x,y)`（不包含 `traits/currentGoal/attributes`）。
- **Proposal 增量**：若需要人格/画像/目标等展示，应先在运行时明确 schema 并通过 `schema_version` 演进。
- HUD 展示模拟时间（step → HH:MM:SS）、世界版本、schema 版本。
- Map/Scene 视图：
  - MapView 显示人格分类颜色/名称标签。
- SceneView 利用 `position` 绘制代理，Portal/交互点可高亮。

## 验收标准
1. 启动 GUI 后，Demo 代理可连续运行≥30 分钟无异常（崩溃/明显泄漏/渲染错位）。
2. Telemetry 与 Inspector 对同一帧数据一致（agents/needs/actions/resources/plannerDecisions 等）。
3. 运行时与 GUI 不跨线程读 ECS（仅消费 Atlas/Telemetry）。

## 里程碑计划（实现顺序）
（以下为 proposal 顺序，用于未来迭代拆解，不代表当前已实现）
1. 引入人格/Traits 组件、属性→需求映射配置，生成多名 Demo 代理。
2. 更新 NeedSatisfier/Planner 以人格权重驱动打分，并按 schema 演进扩展 Telemetry 字段。
3. MapView 标签 + HUD 模拟时间；SceneView 接入局部路径插值。
4. Inspector MVP：展示画像、需求、行动与当前目标摘要。
5. Telemetry 面板（ImPlot）绘制需求曲线；性能指标 HUD。
6. 长时运行实验与回归脚本。

参考文档：
- [`sandbox-gui.md`](./sandbox-gui.md)
- `docs/architecture/agents/agent-model-v0.md`
- `docs/architecture/proposals/agents/agent-personality-big5.md`
- `docs/guides/sandbox_gui_smoke.md`

## 位置与时间语义（设计建议）
- 单位规范：
  - `edge.cost` 表示基础路段代价（推荐作为“时间秒数”或“标准距离”）；
  - `speed` 为倍率（路段时间 = cost / speed），或映射到“距离/秒”的速度标量（需统一度量）。
- 进度编码：
  - Runtime 已有 `MovementState { segmentLength, traveledAlongEdge }` 内部态；建议在 Telemetry 暴露 `movement_progress { entityId, from, to, t01 }`，其中 `t01 = traveledAlongEdge / segmentLength`。
  - 如此即可在 Map View 做边上插值位置，在 Scene View 做入场/离场插值（无须跨线程读 ECS）。
- 节点→场景入口（Portal/Anchor）：
  - 为具备 Tilemap 的节点定义入口/出口锚点（局部坐标），与图上的边对齐；
  - `WorldAtlas::tilemaps` 增补 `portals[{ edgeId|toId, anchor(x,y) }]` 元数据，Scene View 能据此将图边行进投影到局部坐标。
- 纯节点仿真的可行退路：
  - 若无进度与锚点，Map View 以节点中心渲染（当前做法）；
  - Scene View 仅在选中节点时显示静态场景，Agent 统一落在节点中心/默认锚点；
  - 等到需要更高可视化精度时，再增量引入 `movement_progress` 与 `portals`。
