# Proposal · Agent 生命周期（属性→需求→动机→行动→学习→调整）

状态：**Proposal（部分已落地）**。  
本文聚焦把当前零散实现收敛到同一条“行为闭环”语义上，并明确哪些已经落地、哪些仍是设想。已落地规范以 `docs/architecture/agents/agent-model-v0.md` 为准。

## 1. 目标与约束
- **目标**：统一描述 Agent 从“内部状态”到“选择/行动”再到“学习更新”的闭环，使后续扩展（社交、科技、记忆、关系、叙事）仍能落在同一套原子机制组合之上。
- **约束**：
  - **无外力转折**：系统演化应由局部规则与资源/交互反馈驱动；工具链只做观察与对比，不注入剧情。
  - **可复现/可回归**：同 seed/配置必须确定性复现；学习应是可回归验证的（参数更新可被测试捕获）。
  - **避免“模板集合硬选”**：Planner 允许组合/递归生成行动序列，但不依赖预制剧情模板或固定候选计划表。

## 2. 术语（建议统一口径）
- **Attributes（属性）**：连续内部状态（健康、体力、压力、技能等），偏“身体/能力”。
- **Needs（需求）**：从属性与环境推导出的欲求强度与阈值状态（satisfied/critical），偏“生存/偏好/目标压力”。
- **Motives（动机）**：把多个 Need 组织成“此刻要解决什么”的优先级与权衡（例如：先吃再继续生产）。
- **Actions（行动）**：可执行的原子操作（移动、取用、消耗、生产…），可被组合成序列。
- **Outcomes（结果）**：行动的可观测反馈（成功/失败原因、成本、时延、机会损失）。
- **Learning（学习/反思）**：把 outcome 写回到“可学习参数”（而不是直接改世界规则），影响后续 needs/motives/actions。

## 3. 当前已落地子集：在代码里的映射（v0）
当前 core 已经形成最小闭环，但 **Attributes 层尚未独立**（Needs 同时扮演“内部状态”角色）：
- **Needs（内部状态）**：`genesis::agents::NeedComponent / NeedCollection`（`include/genesis/agents/NeedSystem.hpp`、`include/genesis/agents/Needs.hpp`）。
- **Motives（动机/选点）**：`NeedSatisfier` 作为当前“动机生成器”，输出目标并下发补给行动（`src/agents/NeedSatisfier.cpp`）。
- **Actions（行动队列）**：`ActionQueue/ActionExecutor` 执行 `Move/Consume/Take/Produce` 并在缺货时递归补链（`src/agents/ActionSystem.cpp`、`src/agents/ProductionPlanner.cpp`）。
- **Outcomes（反馈）**：失败原因与尝试次数通过 Telemetry/Worldline 被记录（见 `docs/architecture/foundation/telemetry-schema.md`、`docs/architecture/foundation/worldline-chronicle.md`）。
- **Personality（人格）**：Big5 已落地，影响选点评分与确定性 jitter（`include/genesis/agents/Personality.hpp`、`src/agents/NeedSatisfier.cpp`）。
- **Learning（最小学习）**：`AgentExperience.bufferMultiplier[NeedType]` 已落地（`include/genesis/agents/Experience.hpp`），由“关键补给失败/临界抢占”更新并随时间遗忘。

## 4. 学习写回原则（用于后续扩展）
为了避免“预设感”与外力干预，建议学习只写回以下两类：
- **策略参数**（倾向/阈值/缓冲/探索强度/切换粘性）：影响“怎么做”，不直接修改世界规则。
- **模型参数**（对环境的主观估计）：例如对资源点缺货概率/拥挤成本的先验，可由经验更新。

不建议直接学习：
- **剧情脚本/母题触发器**：母题用于分析与对比，不作为 runtime 的显式控制信号。

## 5. 分阶段落地建议（只列近两步）
- **v1：显式 Attributes 组件**（Proposal）
  - 引入 `AgentAttributes`，把“身体/能力/技能”从 Need 中剥离；NeedSystem 变成“从 Attributes + 环境采样推导 Need 强度”的纯函数。
- **v1：Outcome→Learning 事件接口**（Proposal）
  - 为 ActionExecutor/Planner 输出结构化 outcome（成功/失败原因/成本/耗时/中断原因），让学习不依赖散落在各处的分支逻辑。

## 6. 与回归/涌现的关系
- 回归测试验证的是：**原子机制与统计规律是否稳定**（例如：学习确实减少关键补给反复失败、生产链卡死是否消失）。
- 涌现分析依赖 Worldline：把“世界演化历程”作为证据载体，允许跨 seed/配置/版本进行人工归因与母题抽取；工具链负责对齐与对比，不决定剧情走向。

