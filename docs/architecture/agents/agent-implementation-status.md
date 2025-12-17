# Agent 文档-代码对齐矩阵（实施状态）

目的：让“文档 / 想法 / 代码实现”三者对齐。  
本文件只回答一件事：**哪些已经落地、落在哪里；哪些尚未落地、应写入 proposal**。

## 术语
- **Spec**：已实现且作为当前规范（必须与代码一致）。
- **Proposal**：设想/规划/路线图（未实现或仅部分落地），放在 `docs/architecture/proposals/`。

## 对齐矩阵（节选）
| 模块/概念 | 文档来源 | 代码落点（如果有） | 状态 | 备注 |
|---|---|---:|---|---|
| 统一链路：属性→需求→动机→行动→学习 | `docs/architecture/proposals/agents/agent-lifecycle-attributes-needs-learning.md` | 见下方各子项 | Spec | v0 已形成最小闭环；目标态与后续拆分（Attributes/结构化 outcome）仍在 proposal。 |
| Need 数值模型（decay/threshold） | `docs/architecture/proposals/agents/agent-personality-big5.md`（部分） | `include/genesis/agents/Needs.hpp` | Spec | 当前以 Need 本身作为“属性”，无独立 Attributes 层。 |
| NeedSystem 推进 | 同上 | `src/agents/NeedSystem.cpp` | Spec | 仅做线性 decay；无 baseCurve/jitter。 |
| 选点/决策（Planner/Selector） | 同上（理念） | `src/agents/NeedSatisfier.cpp` | Spec | 规则打分 + Big5 权重缩放 + 确定性 jitter（用于打破稳态收敛）。 |
| 经验/学习（最小闭环） | `docs/architecture/proposals/agents/agent-personality-big5.md`（理念） | `include/genesis/agents/Experience.hpp`、`src/agents/LearningSystem.cpp`、`src/agents/NeedSatisfier.cpp` | Spec | 从结构化 outcome（抢占/缺货/规划失败）学习，提高生存 Need 的缓冲倾向并随时间遗忘；同时允许“先补给再继续作业”。 |
| Outcome 缓冲（行动结果事件） | `docs/architecture/proposals/agents/agent-lifecycle-attributes-needs-learning.md` | `include/genesis/agents/Outcomes.hpp`、`src/agents/ActionSystem.cpp` | Spec | ActionExecutor 产出资源尝试/临界抢占等 outcome，供 LearningSystem 消费；与 Telemetry attempt 并行存在。 |
| Beliefs（主观风险估计） | `docs/architecture/proposals/agents/agent-lifecycle-attributes-needs-learning.md` | `include/genesis/agents/Beliefs.hpp`、`src/agents/LearningSystem.cpp`、`src/agents/NeedSatisfier.cpp` | Spec | 目前仅落地“目标交互点 stockout 风险 EMA”，并作为 NeedSatisfier 的风险惩罚项参与评分。 |
| 社交动作（SocializeWithAgent） | 同上（链路） | `include/genesis/agents/ActionSystem.hpp`、`src/agents/ActionSystem.cpp` | Spec | 社交作为一种行动队列任务执行：接近 partner → 定时社交 → 双方降低 Social need → 写入社交 outcome。 |
| 社交信念传播（通过社交事件共享 beliefs） | 同上（链路） | `src/agents/LearningSystem.cpp`、`include/genesis/agents/Outcomes.hpp` | Spec | 社交完成时写入 `SocialInteractionOutcome`，LearningSystem 在同 tick 内对社交 pair 做缺货风险 beliefs 的对称混合（避免顺序依赖）。 |
| 亲和度记忆（AgentRelations） | 未来设想（记忆/关系） | `include/genesis/agents/Relations.hpp`、`src/agents/LearningSystem.cpp`、`src/agents/NeedSatisfier.cpp` | Spec | 最小实现：成功社交增益/失败社交惩罚 + 衰减回 0；并参与社交选伴评分。属于“动力学偏置”，不等同于叙事层“朋友/派系”语义。 |
| 决策可观测性（PlannerDecision） | 同上（链路） | `include/genesis/agents/Planner.hpp` | Spec | 仅用于 Telemetry/拥挤惩罚计数。 |
| 行动队列（Move/Consume/Take/Produce） | 同上（链路） | `include/genesis/agents/ActionSystem.hpp`、`src/agents/ActionSystem.cpp` | Spec | 具备“生产作业时间+临界需求抢占（优先补给，必要时才中断）”。 |
| 自动补链（缺货→上游生产） | 文档未明确 | `src/agents/ProductionPlanner.cpp` | Spec | 当前是工程式 recovery 规划器，不代表“角色策略”。 |
| Movement（直线/跨图 Portal） | 文档提到 | `include/genesis/agents/Movement2D.hpp`、`src/simulation/Movement2DSystem.cpp` | Spec | Tile 级路径未实现。 |
| 工坊配方（meta.workshop） | 文档提到 | worldgen→Runtime 写 meta；执行见 `src/agents/ActionSystem.cpp` | Spec | recipe 选择目前发生在 recovery/执行层。 |
| Big5 人格组件 | `docs/architecture/proposals/agents/agent-personality-big5.md` | `include/genesis/agents/Personality.hpp`、`src/simulation/SimulationContext.cpp` | Spec | Agent 创建时会生成（或由 API 显式指定），并被 NeedSatisfier 读取。 |
| Traits 组件与修饰 | `docs/architecture/proposals/agents/agent-personality-big5.md` | — | Proposal | 目前没有 AgentTraits 组件。 |
| Attributes→Needs 数据驱动映射（JSON/YAML） | `docs/architecture/proposals/agents/agent-personality-big5.md` | — | Proposal | 目前无配置表读取与曲线映射。 |
| 选点 jitter（由人格/stepIndex 驱动） | `docs/architecture/proposals/agents/agent-personality-big5.md`（部分） | `src/agents/NeedSatisfier.cpp` | Spec | 确定性噪声：同 seed/配置可复现；用于引入分歧与转折窗口。 |
| SASCOL（State→Affordance→Selection→Commitment→Outcome→Learning） | `docs/architecture/proposals/agents/agent-model-sascol.md` | `include/genesis/agents/Affordances.hpp`、`src/agents/NeedSatisfier.cpp` | Proposal | 已开始落地 Phase 0：引入 Affordance 载体并在 NeedSatisfier 内以“生成 affordances 列表 → 统一择优”组织候选；Commitment/Negotiation 尚未落地。 |
| 叙事层关系/社交推断（朋友/派系/组织等宏观标签） | 未来设想 | — | Proposal | 仍待“离线观测（lenses）+ 史料事件流”的分析层落地；不应硬编码为 agent 内部语义。 |

## 规范入口
- 当前已落地规范请以 `docs/architecture/agents/agent-model-v0.md` 为准。
- 未落地设想请以 proposal 为准，并在标题显式标注“Proposal/草案/未落地”。
