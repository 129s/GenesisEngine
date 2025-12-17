# Agent 模式 v0（已落地规范）

本文档以当前仓库代码为准，描述 **已经落地** 的 Agent 运行模型（ECS 组件 + 系统职责 + Tick 顺序 + 决策/行动语义）。  
与设想/路线图类内容分离：未来扩展请写入 `docs/architecture/proposals/`。

## 范围与非目标
- 范围：Headless Runtime/Core Simulation（不含 GUI 表现层）。
- 非目标：Traits、显式 Attributes 层、属性→需求的数据驱动映射、叙事系统（这些属于提案，见 `docs/architecture/proposals/`）。

说明：
- v0 已包含最小学习闭环（Experience/Beliefs）与最小社交闭环（SocializeWithAgent + 社交信念传播 + 关系亲和度记忆）。
- 但这些仅作为“行为动力学/学习信号”，不等同于叙事层的“关系/母题/章节”等高阶语义对象。

## 核心链路（当前落地子集）
当前 v0 已经形成可回归验证的最小闭环，但 **Attributes 尚未独立**（Needs 同时扮演内部状态）：
- 内部状态：`NeedComponent/NeedCollection` 随时间推进（NeedSystem）。
- 动机/选点：`NeedSatisfier` 依据 urgency + 评分（含 Big5 权重与确定性 jitter）选择补给目标。
- 行动执行：`ActionExecutor` 执行 `Move/Consume/Take/Produce`；缺货时 `ProductionPlanner` 递归补链。
- 学习写回：
  - `AgentExperience.bufferMultiplier`：从“关键补给失败/临界抢占”学习，影响后续准备阈值与一次性补给量，并随时间遗忘。
  - `AgentBeliefs`：对交互点缺货风险进行 EMA 学习，并参与选点评分（风险惩罚）。
  - `AgentRelations`：对他人形成亲和度记忆（成功社交增益/失败社交惩罚），并参与社交选伴评分。

统一链路的目标态与分阶段落地规划见：`docs/architecture/proposals/agents/agent-lifecycle-attributes-needs-learning.md`。
进一步的统一抽象（去分支化/去硬过滤/显式承诺与协商）见：`docs/architecture/proposals/agents/agent-model-sascol.md`。

## 数据模型（ECS 组件）
- **人格（Big5）**
  - `genesis::agents::AgentPersonalityBig5`：OCEAN（0~1）人格向量（`include/genesis/agents/Personality.hpp`）。
  - 生成方式：Agent 创建时若未显式指定，则按 `entityId + mapId` 的确定性 RNG 生成（可复现、无外部脚本介入）。
- **经验（最小学习）**
  - `genesis::agents::components::AgentExperience`：按 Need 维度记录“缓冲偏好”（`bufferMultiplier`），并随时间遗忘（`include/genesis/agents/Experience.hpp`）。
  - 目前只学习一件事：当工坊作业被“临界需求抢占打断”或关键补给出现反复失败时，会提高对应 Need 的提前准备与一次性补给量（通过 NeedSatisfier 的 prepareMargin / unitsPerRequest 缩放体现）。
- **信念（Beliefs）**
  - `genesis::agents::components::AgentBeliefs`：对交互点的缺货风险进行 EMA 学习（`include/genesis/agents/Beliefs.hpp`）。
  - 写入时机：LearningSystem 消费结构化 outcome（资源尝试）更新；并可在社交时传播。
  - 读取时机：NeedSatisfier 将 `stockoutRisk` 作为风险惩罚项参与评分。
- **关系亲和度记忆（最小社交学习）**
  - `genesis::agents::components::AgentRelations`：对他人形成亲和度记忆（`include/genesis/agents/Relations.hpp`）。
  - 写入时机：LearningSystem 消费社交 outcome（成功/失败）更新，并随时间衰减回中性。
  - 读取时机：NeedSatisfier 在社交选伴评分中加入亲和度项（用于打破“随机社交”并允许形成稳定互动结构）。
- **承诺（Commitment v0）**
  - `genesis::agents::components::AgentCommitment`：对当前选择的目标形成“短期持有”的承诺（`include/genesis/agents/Commitments.hpp`）。
  - 作用：把“保持目标/减少抖动”的行为从特殊分支抽象为承诺对象；切换目标存在 break 代价（在 hold 窗口内更强，窗口外弱化）。
- **社交协商（Meet v0）**
  - `genesis::agents::components::AgentSocialInbox`：收件箱（meet proposal 列表），用于 `propose/accept/timeout` 的最小协商闭环（`include/genesis/agents/Meetings.hpp`）。
  - `genesis::agents::components::AgentSocialMeetState`：会合承诺（Proposed/Accepted），失败原因细分为 Reject/Timeout/NoShow，并写入 outcome 供学习（`include/genesis/agents/Meetings.hpp`、`include/genesis/agents/Outcomes.hpp`）。
- **位置/移动**
  - `genesis::agents::components::AgentLocation2D`：Agent 当前所在 `mapId` 与 2D 坐标（`include/genesis/agents/Movement2D.hpp`）。
  - `genesis::agents::components::MovementIntent2D`：当前运动目标与速度（`include/genesis/agents/Movement2D.hpp`）。
- **需求（Needs）**
  - `genesis::agents::NeedComponent`：持有 `NeedCollection`（需求状态+阈值描述）与上一帧采样（`include/genesis/agents/NeedSystem.hpp`）。
  - `NeedDescriptor/NeedState`：需求数值随时间增长（`decayPerSecond`），达到阈值形成 satisfied/critical（`include/genesis/agents/Needs.hpp`）。
- **决策可观测性**
  - `genesis::agents::components::PlannerDecision`：每 tick 写入的“当前目标/成本/评分”，用于 Telemetry（`include/genesis/agents/Planner.hpp`）。
- **行动队列**
  - `genesis::agents::ActionQueue`：`ActionTask` 队列（`include/genesis/agents/ActionSystem.hpp`）。
  - `ActionType`：`MoveToInteraction / ConsumeResource / TakeResource / ProduceResource / SocializeWithAgent`（同上）。
- **随身资源**
  - `genesis::agents::components::CarriedResources`：按资源类型分槽的随身库存（`include/genesis/agents/CarriedResources.hpp`）。

## 系统职责
- **NeedSystem**：按 `decayPerSecond * deltaSeconds` 推进需求值并产出采样（`src/agents/NeedSystem.cpp`）。
- **NeedSatisfier（当前的“选点/决策”实现）**
  - 读取 Need 状态，计算 urgency，并在可达交互点中选择“最合适”的目标资源点；
  - 写入 `PlannerDecision`，并向 `ActionExecutor` 申请 `ConsumeResource`（`src/agents/NeedSatisfier.cpp`）。
- **LearningSystem（学习系统）**
  - 消费结构化 outcome，更新 Experience/Beliefs/Relations，并进行遗忘/衰减（`src/agents/LearningSystem.cpp`）。
- **ActionExecutor**
  - 执行行动队列；必要时插入 `MoveToInteraction`；
  - `ConsumeResource/TakeResource`：从 `ResourceSystem` 消耗资源，写 Telemetry Attempt；
  - `ProduceResource`：解析工坊配方，消耗输入并向工坊库存产出；
  - `SocializeWithAgent`：接近指定 partner 并进行定时社交；当 partner **接受**（互相选择或 partner 自身也需要社交）时双方降低 Social need；否则记录失败 outcome 作为学习信号（`src/agents/ActionSystem.cpp`）。
  - 若 `ConsumeResource` 因缺货失败，会调用 `ProductionPlanner` 自动补链（`src/agents/ActionSystem.cpp`、`src/agents/ProductionPlanner.cpp`）。
- **Movement2DSystem**
  - 处理 `MovementIntent2D`，在地图内直线移动；跨图时走 Portal（`src/simulation/Movement2DSystem.cpp`）。
- **ResourceSystem**
  - 维护资源点库存：源点 regen、配置的腐败 decay、工坊库存变化（`src/world/system/ResourceSystem.cpp`）。

## Tick 顺序（调度约束）
当前调度顺序为（见 `src/simulation/Scheduler.cpp`）：
0) `LearningSystem.update`：消费结构化 outcome，更新/遗忘学习参数（Experience/Beliefs/Relations）  
1) `NeedSystem.update`：需求随时间上升  
2) `NeedSatisfier.update`：选择目标并下发动作（若已有队列/移动则跳过）  
3) `ActionExecutor.update`：执行 Consume/Take/Produce 与队列推进（并产出 outcome/attempt）  
4) `Movement2DSystem.update`：根据 intent 推进位置（含跨图）  
5) `ResourceSystem.tick`：源点 regen、库存腐败 decay

该顺序意味着：同一 tick 内的生产/消耗会先于 regen/decay 生效（regen/decay 在最后发生）。

## 决策语义（NeedSatisfier）
NeedSatisfier 当前实现是“规则打分 + 人格扰动”（非 Traits、非数据驱动映射）：
- 触发条件：当某 Need 超过 `satisfiedThreshold + prepareMargin` 或已经 critical，才会尝试补给。
- 评分要素：
  - `travelCost`：同图欧式距离平方；跨图为最短路径 `totalCost + crossMapPenalty`（跨图惩罚受 `openness` 缩放）。
  - `scarcityPenalty`：资源点越空惩罚越大（按 `1 - current/capacity` 线性映射；权重受 `neuroticism/conscientiousness/openness` 影响）。
  - `crowdPenalty`：目标越拥挤惩罚越大（按其他 Agent 的 `PlannerDecision.target` 计数；权重受 `neuroticism/extraversion/agreeableness` 影响）。
  - `riskPenalty`：对目标交互点的主观缺货风险惩罚（来自 `AgentBeliefs.stockoutRisk`；权重受人格缩放）。
  - `decisionJitter`：每次选点会注入小幅确定性噪声（由 `stepIndex` 与 `entityId` 导出），用于打破“全体收敛到同一最优点”的稳态。
- 社交选伴（Need=Social）额外要素：
  - 候选集：同地图的其他 Agent（不再硬过滤 partner 的移动/待执行动作；改为在评分里降低“可用性/成功率”，为后续显式承诺/协商机制留接口）。
  - `partnerBonus`：偏好与“也想社交”的 partner 互动（更自然的相遇）。
  - `affinityBonus`：偏好与历史亲和度更高的 partner 互动（来自 `AgentRelations`）。
- 目标承诺（Commitment v0）：
  - NeedSatisfier 会把当前选择写入 `AgentCommitment` 并设置短期 `holdUntilStep`；
  - 在 hold 窗口内切换目标需要克服更大的 break 代价（通过 score margin 表达），窗口外切换更容易。
- 行为倾向（宏观效果）：
  - 高 `conscientiousness/neuroticism`：更早开始准备（更大的 prepareMargin）、更难切换目标（更高 switch margin）。
  - 高 `openness`：跨图成本更低、噪声更大（更“游走/探索”）。
- 默认参数见 `include/genesis/agents/NeedSatisfier.hpp`（如 `crossMapPenalty=500`、`demandPenaltyPerAgent=60` 等）。

## 工坊与生产
- 工坊通过 `Interaction.meta.workshop` 描述 recipes（运行时由 worldgen 配置写入 meta）。
- `ProduceResource` 在工坊执行 recipe：
  - 输入分为 consumable/非 consumable；非 consumable 只要求持有，不按批次消耗。
  - 产出写入工坊库存（受容量限制）。
- `ConsumeResource` 若因缺货失败，会触发 `ProductionPlanner`：
  - 规划器会尝试为“目标资源”递归补齐上游输入（深度受限），并生成一串 `Produce/Take/Consume` 动作。
  - recipe 选择为启发式（估算“补齐缺口的最短路成本”）。

## Telemetry（可观测基线）
TickTelemetry 中与 Agent 模式直接相关的快照：
- `needs[]`：NeedSnapshot（needName/value/critical）
- `plannerDecisions[]`：PlannerSnapshot（target/travelCost/score）
- `actions[]`：ActionSnapshot（当前队列头动作与参数）
- `workshopAttempts[]`、`resourceAttempts[]`：生产/取用尝试与失败原因  
见 `include/genesis/telemetry/TelemetryBuffer.hpp` 与 `docs/architecture/foundation/telemetry-schema.md`。
