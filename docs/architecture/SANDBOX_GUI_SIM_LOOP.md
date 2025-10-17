# Sandbox GUI · 完整模拟循环与多性格代理（提案）

目标：在现有 Sandbox GUI（GLFW + ImGui + RuntimeBridge）基础上，补齐“完整模拟循环”所需的要素，并引入 3 个决策倾向不同的 Agents，使其能在小镇场景中长期自主运行与可视化观测。

## 当前实现摘要
- 线程/数据流
  - RuntimeBridge：后台线程推进 `genesis::runtime::Runtime`，支持 Pause/Step/Speed，多帧快照环形缓冲（默认 96）。
  - GUI 线程：消费最新 `TickTelemetry` 快照，渲染 World View / Telemetry / Log / Status Bar。
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

2) 多性格 Agents（核心）
- 建模：为 Agent 增加“决策偏好”组件（Personality），在规划/选址时影响评分函数权重与阈值。
- 生成：Engine 启动时生成 3 个带不同 Personality 的 Agent。
- 可视化：在 World View 中以同一图例渲染，但支持可选显示 ID/名称以便区分（可选）。

## Personality 建模（建议）
新增组件 `AgentPersonality`（挂在每个 Agent 上），示例字段：
- `travelWeight`：路程代价权重（越重越偏好近处）。
- `occupancyWeight`：拥挤惩罚权重（越重越避开拥挤）。
- `stockWeight`：库存惩罚权重（越重越偏好高存量）。
- `prepareMargin`：饥饿预备阈值偏移（越高越“未雨绸缪”）。
- `baseSpeed`：基础移动速度（影响 MovementIntent）。
- `decisionJitter`：决策噪声（探索性）。

评分函数（用于 HungerPlanner 的选址）：
- `score = travelWeight * travelCost + occupancyWeight * occPenalty + stockWeight * stockPenalty + noise(decisionJitter)`
- 由 Personality 注入权重；无组件时使用默认值（保持现有行为）。

建议预设三种性格（可修改命名）：
- 快餐派（Greedy-Nearest）：`travelWeight=1.5, occupancyWeight=0.2, stockWeight=0.3, prepareMargin=5, baseSpeed=1.2`
- 避拥挤（Crowd-Averse）：`travelWeight=1.0, occupancyWeight=2.0, stockWeight=0.5, prepareMargin=3, baseSpeed=1.0`
- 囤积狂（Stock-Pref）：`travelWeight=0.6, occupancyWeight=0.2, stockWeight=2.5, prepareMargin=8, baseSpeed=0.9`

## 规划/系统接入点
- Planner：在 `HungerPlanner` 的 locator 中读取 Personality 权重，替换/扩展现有 `occupancyPenalty/stockPenalty` 逻辑为“可加权评分”。
- Need 执行：通过 `NeedSatisfierConfig.hungerPreferredLocator` 保持现有线程边界，内部调用 Personality 感知的 locator。
- Movement：当生成 `MoveTo` 任务或 `MovementIntent` 时，使用 `baseSpeed` 作为默认速度。
- Engine：`spawnDemoAgents()` 生成 3 个 Agent，并为其附加不同 Personality；按需设置初始 needs 状态差异以增强对比。

## Telemetry/GUI 增量（可选）
- Telemetry：为 `AgentSnapshot` 增加 `persona` 或 `name`（可选）；或 GUI 侧仅显示 entityId。
- 插值坐标：若需平滑轨迹，可在 Telemetry 增加 `edgeProgress/segment` 字段；本阶段可暂不实现。
- Inspector：后续阶段可在 Inspector 面板中展示 Personality 字段（非本阶段必需）。

## 验收标准（本阶段）
- 启动 Sandbox GUI 后：
  - 可见 3 个 Agent 在小镇中自主寻找食物，行动状态与轨迹正常更新。
  - 在“多食物点”条件下，三者在拥挤/距离/存量权衡上体现不同选择（见下方数据条件）。
  - 长时间运行（≥30 分钟）无崩溃/明显泄漏。

## 数据条件与小镇场景
- 现有 demo_world 仅有 1 个 Food 产出（厨房）。为验证不同性格的选择差异，建议：
  - 方案 A：在 `Town Center` 或 `Residential Block A` 新增第二个 `Food` 产出（较低/较高库存），或调整边权重产生明显距离差异。
  - 方案 B：保持单点，但通过 occupancy 惩罚 + 速率/容量差异，观察“避拥挤 vs 近处优先”的分流效果（区分度较弱）。

## 里程碑计划（实现顺序）
1) 引入 `AgentPersonality` 组件与默认权重；`spawnDemoAgents()` 生成 3 个不同性格的 Agent。
2) `HungerPlanner` 定制 locator：读取 Personality 权重并计算评分；`NeedSatisfier` 透传。
3) `ActionExecutor/Movement` 使用 `baseSpeed`；在 Telemetry/Log 中打印决策摘要（便于验证）。
4) 可选：为 demo_world 新增第二个 Food spawn 或调整边权重，以增强对比。
5) 文档与指南更新：使用说明、验收步骤、已知限制。

## 待确认问题（请反馈）
- 性格维度：上述权重/字段是否满足你的预期？是否需要加入“社交/能量偏好”的跨 Need 权重？
- 数据改动：是否接受在 demo_world 中新增一个 Food spawn（位置/容量/产率）以增强可观察性？
- 可视化：是否需要在 World View 中显示 Agent 名称/性格标签？（当前仅显示实体标记）
- 插值：本阶段是否需要在 GUI 展示更平滑的移动（需要 Telemetry 扩展），还是暂用离散节点即可？
- 时间标度：每 step 作为模拟时间的语义（如 0.5s=1 step）是否需要在 UI 中以“分钟/小时”显示？

---
参考文档：
- `docs/architecture/SANDBOX_GUI.md`
- `docs/roadmap/SANDBOX_GUI.md`
- `docs/guides/sandbox_gui_smoke.md`
