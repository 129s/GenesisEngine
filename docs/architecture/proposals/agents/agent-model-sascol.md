# Proposal · Agent 模型（State/Affordance/Selection/Commitment/Outcome/Learning）

状态：**Proposal（未落地）**。  
本文把 Agent 的决策与学习闭环统一表达为：

> `State → Affordance → Selection → Commitment → Outcome → Learning → (State update)`

它与当前 v0 最大差异在于：
- 不再为 `Hunger/Thirst/Social` 写三套近似触发器/分支逻辑；
- 不再用硬编码 `if`/过滤把世界剪成“只剩一个候选”，而是把这些降解为**统一的状态、约束与成本**；
- “叙事结构”不写进 agent（例如朋友/同事/派系），只输出可观测事实供离线分析层派生。

已落地规范以 `docs/architecture/agents/agent-model-v0.md` 为准。

## 1. 目标与约束

- **原子与正交**：复杂度来自原子机制的组合，而不是语义近似的重复模块（social/hunger/thirst 的“换皮触发器”）。
- **无预制剧情/无外力**：系统转折来自局部规则与反馈；工具链只观测对比，不注入剧情。
- **可复现/可回归**：同 seed/配置必须确定性；学习更新必须能被回归捕获。
- **可扩展实体关系**：支持“人与人/人与物/人与制度”的统一处理（例如“我是公司程序员”）。

## 2. 概念与职责

### 2.1 State（内部状态：微观内因）

State 是 agent 的“内因容器”，只负责**存储**，不直接决定分支。

- `homeostats[]`：一组连续维度（可理解为“稳态偏离量”）。你仍然可以叫 hunger/thirst/social，但它们只是维度名称，不对应分支逻辑。
- `beliefs(entity)`：对实体/地点/制度/他人的可预测属性估计（成功率、缺货率、耗时、收益分布、可靠性…）。
- `attitudes(entity)`：对实体的偏好/信任/义务等价值偏置（可视为潜意识倾向，不要求自知）。
- `commitments`：当前承诺（我正在做什么、可否打断、预计完成时间、我答应了谁什么）。

与当前 v0 的映射（仅对照，不代表已落地）：
- `homeostats[]` ≈ v0 的 `NeedComponent/NeedCollection`
- `beliefs(interaction.stockoutRisk)`：v0 已落地子集（`AgentBeliefs.stockoutRiskEma`）
- `attitudes/commitments`：v0 未落地

### 2.2 Affordance（外部可供性：世界自然长出的行动原材料）

Affordance 是由“局部可感知世界状态 + 规则”生成的**可能性清单**，不是剧情模板集合。

例：
- `consume@ResourceNode(X)`
- `take@ResourceNode(X)`
- `produce@Workshop(Y)`
- `travel@Location(Z)`
- `propose_meet@Agent(B)` / `wait_for@Agent(B)`（若规则允许通讯/近距离）
- `trade@Agent(B)`（若交易机制存在）

关键点：
- Affordance 的生成是系统性的：来自**附近有什么、规则允许什么、自己是否能做到**。
- 不做“把世界剪到只剩一个候选”的硬过滤：例如对方在移动不应直接剔除，应该体现在 success 概率下降、等待成本上升、需要协商等。

### 2.3 Selection（统一选择器）

Selection 对所有 affordance 用同一套评估函数选择下一步：

> 期望净效用 `U(a)` = 预期收益 `E[benefit]` − 成本 `cost` − 风险 `risk` + 偏置 `bias`

其中：
- `E[benefit]` 由 `homeostats[]` 的权重调节：越“口渴”，与补水相关的收益权重越大；但仍是同一公式。
- `risk` 由 `beliefs(entity)` 提供（例如缺货概率、爽约概率、失败率）。
- `bias` 由 `attitudes(entity)` 提供（偏好/信任/义务）。
- `commitments` 以约束/惩罚形式进入：承诺违约的代价、可中断性、剩余工期等。

确定性探索（避免全体收敛到同一最优）：
- 允许在 `U(a)` 上叠加**确定性噪声**（由 seed/entityId/stepIndex 导出），保持可复现，同时为涌现提供分歧来源。

### 2.4 Commitment / Negotiation（承诺与协商）

Commitment 是把“计划”从隐式队列升级为可被学习与约束的对象：
- 对单人行动：把当前行动队列/正在做的事视为 commitment（含可中断性、预计完成时间、违约代价）。
- 对多人行动：先 `propose` 再形成双方 commitment；拒绝/爽约/延期都成为可学习的 outcome。

“我是公司程序员”的统一表达：
- `Company` 是一个实体；“雇佣关系”是对 Company 的长期 contract commitment（任务/报酬/约束），而不是人-人关系特例。

### 2.5 Outcome（执行/结果）

Outcome 是对执行的可观测反馈（成功/失败原因、耗时、消耗、机会损失、违约/延期等），用于：
- 进入 Learning 更新 beliefs/attitudes/commitments 参数；
- 写入可观测事实流（eventline），供离线叙事分析层派生宏观结构。

### 2.6 Learning（学习/更新）

Learning 的职责是把 outcome 写回可学习参数（“怎么做/怎么预期”），而不是写剧情。

- `beliefs(entity)`：更新成功率/缺货率/耗时分布等（EMA/贝叶斯更新等均可，只要可回归）。
- `attitudes(entity)`：互惠/背叛/违约对偏置的塑形（例如信任下降、义务感上升）。
- `commitments`：更新“承诺可靠性”的主观估计，以及自身对中断/拖延的代价权重。
- `homeostats`：允许学习“缓冲偏好/提前准备/切换粘性”等策略参数（类似 v0 的 `bufferMultiplier`，但语义更统一）。

## 3. 典型链路（例：口渴取水 + 顺便社交）

1) `homeostats` 更新：口渴维度上升（只是数值变化，不触发专用 if）。  
2) 感知生成 affordances：看到附近 `consume@well_3`、远处 `travel@well_8`、路过某个 agent 有 `propose_meet@B` 等。  
3) `beliefs` 参与预测：`beliefs(well_3)` 认为缺货/拥挤高；`beliefs(well_8)` 更稳但路远；`beliefs(B)` 认为常爽约。  
4) `attitudes` 参与偏置：即使 `beliefs(B)` 一般，`attitudes(B)` 可能仍让会面值得（或反之）。  
5) Selection 统一评估：选择 `travel@well_8`（期望净效用最高）；并可能发起 `propose_meet@B`。  
6) Commitment 形成与冲突：提议被拒绝→形成社交失败 outcome；接受→双方生成在 `t+Δ` 于地点 P 会合的 commitment。  
7) 执行与结果：取水成功/失败缺货；会合成功/失败爽约。  
8) Learning 更新：更新 `beliefs(well_8)`/`beliefs(B)`，并调整 `attitudes/commitments`。  
9) 宏观结构（外部派生）：分析器从大量轨迹派生“朋友/组织/派系”等母题标签；这些属于叙事层史书内容，不写进 agent。

## 4. 分阶段落地建议（兼容 v0，避免“一口吃成胖子”）

### Phase 0：把现有行为表述成 SASCOL（不改机制，只改语义边界）
- 把 v0 的 “NeedSatisfier 选点 + ActionQueue 执行 + LearningSystem 更新”映射到：
  - State：Needs/Beliefs/Relations（已落地子集）
  - Affordance：从世界数据库/附近交互点枚举的动作候选（等价于当前遍历交互点）
  - Selection：当前打分函数就是统一选择器雏形
  - Commitment：当前 action queue（先当作隐式 commitment）
  - Outcome/Learning：现有结构化 outcome 与学习写回

当前落地进度（截至本提案最后更新）：
- 已引入最小 `Affordance` 数据结构：`include/genesis/agents/Affordances.hpp`
- `NeedSatisfier` 内部已以“生成 affordances 列表 → 统一择优”的方式组织候选：`src/agents/NeedSatisfier.cpp`

### Phase 1：去分支化（同一套公式覆盖 hunger/thirst/social）
- 让 “需求维度” 只影响权重/成本曲线，不引入新的 if 分支。
- 用 “affordance 的属性”描述行为差异（资源类型、单位收益、等待/移动成本），而不是写三套触发器。

### Phase 2：显式 Commitment（含可中断性/违约代价/预计完成时间）
- 把“临界抢占/切换粘性”从特殊逻辑抽象为 commitment 的统一约束。
- 为后续“协商/交易/合同”打底。

当前落地进度：
- Commitment v0 已落地“目标持有窗口 + 切换代价（score margin）”：`include/genesis/agents/Commitments.hpp`、`src/agents/NeedSatisfier.cpp`
  - v0 仍未覆盖：协商/违约后果/显式预计完成时间（仅有 holdUntilStep 作为短期约束）

### Phase 3：Negotiation（多 agent 的提议/接受/爽约）
- 取消“对方在移动就过滤掉”的硬规则；用 success 概率、等待成本与违约后果表达。
- 引入 outcome：拒绝/爽约/延期，进入 beliefs/attitudes 学习。

## 5. 非目标（明确避免）

- 不在 runtime 内置“章节/标题/母题词表/剧情注入”。
- 不把“朋友/同事/组织”这类宏观标签硬编码为 agent 内部语义（只允许离线分析派生）。
- 不引入“预制计划模板集合”让 agent 选剧本；affordances 必须来自环境与规则的系统性枚举。
