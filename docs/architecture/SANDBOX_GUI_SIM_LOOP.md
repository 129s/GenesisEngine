# Sandbox GUI · 完整模拟循环与多性格代理（提案）

目标：在现有 Sandbox GUI（GLFW + ImGui + RuntimeBridge）基础上，补齐“完整模拟循环”所需的要素，并引入 3 个基于大五人格（OCEAN）的 Agents，使其能在小镇场景中长期自主运行与可视化观测。

## 当前实现摘要
- 线程/数据流
  - RuntimeBridge：后台线程推进 `genesis::runtime::Runtime`，支持 Pause/Step/Speed，多帧快照环形缓冲（默认 96）。
  - GUI 线程：消费最新 `TickTelemetry` 快照，渲染 Map View / Telemetry / Log / Status Bar。
- 世界/可视化
  - WorldAtlas：基于 `WorldRegistry` 的分层节点布局（Region/Building/Room/Point），渲染节点/边/资源生成点。
  - Agent Overlay：按快照绘制 Agent 标记与（可选）轨迹，Action 状态着色（Move/Consume/Idle/Other）。
- 世界数据
  - `data/world/demo_world.json`：包含镇中心、酒馆（含厨房/大厅）、住宅、宿舍；食物产出于厨房；社交产出于大厅。

局限/缺口
- 仅在 Engine 启动时生成 1 个 Demo Agent；无“个体性格/偏好”建模。
- HungerPlanner 使用统一 locator 规则，未按个体差异加权；无法体现“不同决策倾向”。
- Agent 位置仅为离散节点（缺少边上插值坐标）；Inspector/选择等交互尚未接入（不影响本阶段）。

## 目标拆解
1) 完整模拟循环（GUI 视角）
- 播放控制：Pause/Resume/Step/Speed（已具备）。
- 快照与指标：最新帧摘要（已具备），后续可扩展历史曲线（非本阶段必需）。
- 长时间稳定：1h+ 运行稳定、UI 解耦（已契合设计，需以验证为准）。

2) 多性格 Agents（核心，MVP 范围）
- 建模：为 Agent 增加“大五人格组件（OCEAN）”，在觅食等评分函数中注入通用权重与偏好（其他行为后续扩展）。
- 生成：Engine 启动时生成 3 个带不同 OCEAN 画像的 Agent（名称随意，后续引入名称生成器）。
- 可视化：Map View 仅展示 Agent 名称标签（不展示轨迹，默认关闭复杂叠加）。
- 调试：Inspector 不作为 MVP 交付，列入后续里程碑；MVP 以日志与名称标签辅助验证。

## 人格建模（大五 · OCEAN）
- 组件：`AgentPersonalityBig5 { openness, conscientiousness, extraversion, agreeableness, neuroticism }`，取值 0.0~1.0。
- 通用映射（示例）：
  - 探索倾向：↑openness → 更偏好探索/新地点（降低“距离代价”权重，增加随机探索抖动）。
  - 效率/计划：↑conscientiousness → 提前准备（提高准备阈值/计划视野），偏好近/省时路线。
  - 社交偏好：↑extraversion → 更频繁社交活动/选择拥挤地点；↓extraversion → 避拥挤。
  - 合作/利他：↑agreeableness → 倾向贡献/让渡资源（后续用于交易/协作）。
  - 风险规避：↑neuroticism → 避险/囤货/远离拥挤，提高库存偏好权重与准备阈值。
- 评分示例（觅食选址）：`score = w_dist * travelCost + w_occ * occupancyPenalty + w_stock * stockPenalty + jitter`，其中
  - `w_dist = 1.4 + 0.8*C - 0.8*O`
  - `w_occ = 0.4 + 1.6*N - 1.0*E`
  - `w_stock = 0.4 + 1.0*N + 0.6*C - 0.4*O`
  - `prepareMargin = base + 6*C + 6*N`
  - `jitter ~ U(-σ,σ), σ = 0.2*O - 0.1*C`
  - 默认无组件时退化为当前行为（与现有结果一致）。
- 三个示例画像（初始 Demo）：详见 `docs/architecture/AGENT_PERSONALITY_BIG5.md`。

## 规划/系统接入点
- Planner：在 `HungerPlanner` 的 locator 中读取 Personality 权重，替换/扩展现有 `occupancyPenalty/stockPenalty` 逻辑为“可加权评分”。
- Need 执行：通过 `NeedSatisfierConfig.hungerPreferredLocator` 保持现有线程边界，内部调用 Personality 感知的 locator。
- Movement：当生成 `MoveTo` 任务或 `MovementIntent` 时，使用 `baseSpeed` 作为默认速度。
- Engine：`spawnDemoAgents()` 生成 3 个 Agent，并为其附加不同 Personality；按需设置初始 needs 状态差异以增强对比。

## Telemetry/GUI 增量
- Telemetry：为 `AgentSnapshot` 增加 `name` 字段（暴露名称）；
- Map View：仅显示代理名称（隐藏轨迹/路径，保留 Debug 切换）。
- Step 语义化：按 `SimulationClock` 将 step 映射到“模拟时间”（默认先用 0.5s/step 的凑数值），在 HUD/状态栏显示 HH:MM:SS。
- 插值（低优先级）：保留 Telemetry 扩展接口设计，但默认不在 Map View 上渲染大规模路径/轨迹；小图场景建议在 Scene View 呈现微观行动。

## 验收标准（本阶段）
- 启动 Sandbox GUI 后：
  - 可见 3 个 Agent 在小镇中自主寻找食物，行动状态与轨迹正常更新。
  - 在“多食物点”条件下，三者在拥挤/距离/存量权衡上体现不同选择（见下方数据条件）。
  - 长时间运行（≥30 分钟）无崩溃/明显泄漏。

## 数据条件与小镇场景
- 同意新增一个 Food 产出点：建议加于 `Town Center`（近/低容量/高拥挤）或 `Residential Block A`（远/高容量/低拥挤），用于验证 OCEAN 对距离/拥挤/存量的权衡影响。

## 里程碑计划（实现顺序）
1) 引入 `AgentPersonalityBig5` 组件与 3 个示例画像；Engine 侧生成并命名三名 Agent（名称暴露至 Telemetry）。
2) HungerPlanner 定制 locator：从 OCEAN 计算通用权重，评分并选址；NeedSatisfier 透传。
3) Map View 名称标签与 Step 语义化：状态栏/HUD 显示模拟时间；提供倍率配置（初值先凑数）。
4) 增加第二个 Food spawn（数据文件更新），并记录 A/B 实验建议。
5) 文档与指南更新：使用说明、验收步骤、已知限制。
6) Inspector 面板：后续里程碑实现（非 MVP）。

## 设计取舍小结
- 人格采用大五（OCEAN），并以通用映射影响多类行为，不局限于觅食。
- Map View 作为概览/小地图：仅名称与概况，不在大图渲染大规模路径。
- 微观行动信息通过 Scene View 呈现；路径插值为低优先级 Debug 能力。
- Inspector 延后至后续里程碑，MVP 使用名称标签与日志完成验证。

---
参考文档：
- `docs/architecture/SANDBOX_GUI.md`
- `docs/roadmap/SANDBOX_GUI.md`
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
