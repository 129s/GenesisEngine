# Agent 文档-代码对齐矩阵（实施状态）

目的：让“文档 / 想法 / 代码实现”三者对齐。  
本文件只回答一件事：**哪些已经落地、落在哪里；哪些尚未落地、应写入 proposal**。

## 术语
- **Spec**：已实现且作为当前规范（必须与代码一致）。
- **Proposal**：设想/规划/路线图（未实现或仅部分落地），放在 `docs/architecture/proposals/`。

## 对齐矩阵（节选）
| 模块/概念 | 文档来源 | 代码落点（如果有） | 状态 | 备注 |
|---|---|---:|---|---|
| Need 数值模型（decay/threshold） | `docs/architecture/proposals/agents/agent-personality-big5.md`（部分） | `include/genesis/agents/Needs.hpp` | Spec | 当前以 Need 本身作为“属性”，无独立 Attributes 层。 |
| NeedSystem 推进 | 同上 | `src/agents/NeedSystem.cpp` | Spec | 仅做线性 decay；无 baseCurve/jitter。 |
| 选点/决策（Planner/Selector） | 同上（理念） | `src/agents/NeedSatisfier.cpp` | Spec | 当前是固定规则打分；不使用 Big5/Traits。 |
| 决策可观测性（PlannerDecision） | 同上（链路） | `include/genesis/agents/Planner.hpp` | Spec | 仅用于 Telemetry/拥挤惩罚计数。 |
| 行动队列（Move/Consume/Take/Produce） | 同上（链路） | `include/genesis/agents/ActionSystem.hpp`、`src/agents/ActionSystem.cpp` | Spec | 具备“生产作业时间+临界需求抢占中断”。 |
| 自动补链（缺货→上游生产） | 文档未明确 | `src/agents/ProductionPlanner.cpp` | Spec | 当前是工程式 recovery 规划器，不代表“角色策略”。 |
| Movement（直线/跨图 Portal） | 文档提到 | `include/genesis/agents/Movement2D.hpp`、`src/simulation/Movement2DSystem.cpp` | Spec | Tile 级路径未实现。 |
| 工坊配方（meta.workshop） | 文档提到 | worldgen→Runtime 写 meta；执行见 `src/agents/ActionSystem.cpp` | Spec | recipe 选择目前发生在 recovery/执行层。 |
| Big5 人格组件 | `docs/architecture/proposals/agents/agent-personality-big5.md` | `include/genesis/agents/Personality.hpp` | Proposal | 结构存在，但未被任何系统读取/写入。 |
| Traits 组件与修饰 | `docs/architecture/proposals/agents/agent-personality-big5.md` | — | Proposal | 目前没有 AgentTraits 组件。 |
| Attributes→Needs 数据驱动映射（JSON/YAML） | `docs/architecture/proposals/agents/agent-personality-big5.md` | — | Proposal | 目前无配置表读取与曲线映射。 |
| Need jitter（由人格/seed 驱动） | `docs/architecture/proposals/agents/agent-personality-big5.md` | — | Proposal | 当前没有随机扰动层。 |
| 记忆/关系/社交推断 | 未来设想 | — | Proposal | 尚未进入 core。 |

## 规范入口
- 当前已落地规范请以 `docs/architecture/agents/agent-model-v0.md` 为准。
- 未落地设想请以 proposal 为准，并在标题显式标注“Proposal/草案/未落地”。
