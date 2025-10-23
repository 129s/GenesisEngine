# Sandbox GUI · 模拟循环与多人格代理

目标：在 Sandbox GUI 中展示完整的模拟循环，验证“属性→需求→决策→行动”链路与多人格（OCEAN + Traits）代理的可视化表现，确保长时间运行稳定。

## 当前实现摘要
- 线程/数据流
  - RuntimeBridge：后台线程推进 Runtime，支持 Pause/Step/Speed，维护环形 Telemetry 缓冲（默认 96 帧）。
  - GUI 线程：消费最新快照，驱动 Map/Scene/Telemetry/Inspector 面板。
- 世界/可视化
  - WorldAtlas：提供 Scene/Interactive 节点、`coord_global/coord_local`、Portal 锚点与 Tilemap 元数据；MapView 绘制节点拓扑，SceneView 渲染 `insideView` 并高亮资源/Portal。
  - 代理覆盖层：按 Telemetry 中的 `mapId + localPosition` 绘制，支持名称标签、状态着色。
- 世界数据
  - Demo 世界基于 MapConfig 生成：主街区 + Tavern 内景 + 住宅区 + 森林边缘，包含至少两个食物点验证人格差异。

## 主要缺口
- 仅加载单个 Demo Agent，尚未验证人格/Traits 差异。
- Hunger Planner 使用统一权重，无法体现属性→需求→决策链的差异化。
- 运动仅在节点级插值，缺少 Tile 层路径同步；Inspector 功能待完善。

## 目标拆解
1. **模拟循环可视化**
   - 播放控制：Pause/Resume/Step/Speed（已具备）。
   - Telemetry 面板：展示属性、需求强度、系统耗时曲线。
   - Map/Scene 视图同步：Portal/锚点高亮，世界版本切换时正确刷新。
   - 稳定性：连续运行 ≥1h 无崩溃、内存泄漏、渲染错误。
2. **多人格代理（核心）**
   - 引入 `AgentPersonalityBig5` + `AgentTraits` 组件，驱动属性→需求→决策链。
   - Engine 启动生成至少 3 名代理（Explorer/Planner/Socializer），命名暴露给 Telemetry。
   - Planner/Selectors 使用人格/Traits 权重选择资源/社交/探索目标。
   - GUI 展示人格差异：Map/Scene 标签、Inspector 人格与当前目标、Telemetry 需求曲线。
3. **局部路径同步（可见范围）**
   - GUI 可视范围内的代理执行 Tile 层 A*，并将位置插值映射到 SceneView。
   - 逻辑寻路保持权威；若可视化滞后，允许 snap 回逻辑锚点。

## 人格链路回顾
- 属性：`health/sanity/hunger/energy` 等按系统更新。
- 需求评估：根据 `NeedDefinition`（属性权重 + 人格系数 + Trait 修改）计算强度。
- 决策：按需求强度与人格偏好选择目标节点（Portal/交互点）。
- 行动：Planner 生产行动序列（Move、Interact、Wait），Movement 系统执行。
- 详细映射见 [agents/agent-personality-big5.md](../../agents/agent-personality-big5.md)。

## 系统接入点
- **Attributes/Needs 系统**：加载配置表，输出需求强度与阈值。
- **DecisionSystem**：读取人格/Traits，挑选目标（资源、社交、探索、休息）。
- **Planner**：对目标节点执行评分 → 生成行动队列。觅食示例公式见人格文档。
- **Movement**：
  - 逻辑层：计算 Portal 链并更新 `movementProgress`。
  - 可视化层：在可见 map 上运行 Tile 层 A*，更新 `localPosition`。
- **Engine 初始化**：`spawnDemoAgents()` 生成 Demo NPC 并设置初始属性差异。

## Telemetry / GUI 增量
- `TickTelemetry.agents[]` 新增：`mapId`、`localPosition`、`currentGoal`、`traits`。
- `needs[]` / `attributes[]` 输出用于 Telemetry 曲线；Inspector 读取人格/Traits。
- HUD 展示模拟时间（step → HH:MM:SS）、世界版本、schema 版本。
- Map/Scene 视图：
  - MapView 显示人格分类颜色/名称标签。
  - SceneView 利用 `localPosition` 绘制代理，Portal/锚点可高亮。

## 验收标准
1. 启动 GUI 后，三名 Demo 代理在城镇中自主运行≥30 分钟无异常。
2. 不同人格在拥挤/距离/库存权衡上呈现差异（例如 Explorer 偏向远处资源，Planner 偏向近处稳定点）。
3. Telemetry 曲线显示属性/需求变化，并与 Inspector 数据一致。
4. SceneView 在可见 map 内展示 Tile 层路径，逻辑与可视化进度差距不超过一个 Simulation step。

## 里程碑计划（实现顺序）
1. 引入人格/Traits 组件、属性→需求映射配置，生成三名 Demo 代理。
2. 更新 Planner（觅食/社交）以人格权重驱动评分，扩展 Telemetry 字段。
3. MapView 标签 + HUD 模拟时间；SceneView 接入局部路径插值。
4. Inspector MVP：展示人格、属性、需求、当前目标。
5. Telemetry 面板（ImPlot）绘制属性/需求曲线；性能指标 HUD。
6. 长时运行实验与回归脚本。

参考文档：
- [`sandbox-gui.md`](./sandbox-gui.md)
- [`../../agents/agent-personality-big5.md`](../../agents/agent-personality-big5.md)
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
