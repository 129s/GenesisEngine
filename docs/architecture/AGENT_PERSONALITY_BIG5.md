# Agent Personality · OCEAN + Traits

## 概述
- 目的：建立“属性 → 需求 → 决策 → 行动”链路中人格与特质的统一建模，支持多需求、多系统扩展。
- 模型：
  - `Attributes`：连续状态（health, sanity, hunger, energy …）。
  - `Needs`：由属性、人格、Trait 与环境驱动的欲求（social、rest、food、explore…）。
  - `Personality`：大五人格（OCEAN，0~1），组件 `AgentPersonalityBig5 { openness, conscientiousness, extraversion, agreeableness, neuroticism }`。
  - `Traits`：离散/枚举标签（如 `night_owl`、`gourmet`）调节需求或解锁特殊行动。
- 逻辑链：`Attributes` 更新 → `NeedEvaluation` 计算强度 → `DecisionSystem` 选取目标 → `ActionPlanner` 生成计划。

## 属性与需求映射
- 每个 Need 定义 `NeedDefinition`：
  ```json
  {
    "needId": "hunger",
    "baseCurve": { "type": "sigmoid", "k": 6.0, "x0": 0.4 },
    "attributeWeights": { "hunger": 1.0, "energy": 0.2 },
    "personalityWeights": { "O": -0.4, "C": 0.6, "N": 0.5 },
    "traitModifiers": { "gourmet": 0.2, "ascetic": -0.3 },
    "jitter": { "sigmaBase": 0.05, "sigmaByO": 0.15, "sigmaByC": -0.1 }
  }
  ```
- 评估公式示例：
  ```
  intensity = sigmoid(baseCurve, Σ attrWeight * attributeValue)
              + Σ personalityWeight * OCEAN
              + Σ traitModifier
              + jitter(Personality, seed)
  ```
- 需求阈值（`threshold`, `prepareMargin`）同样可由人格/Traits 调节，例如：
  - `threshold = base + 0.6 * conscientiousness + 0.6 * neuroticism`
  - `prepareMargin = baseMargin + traitBonus`

## 决策与行动
- DecisionSystem 读取需求强度排序，结合人格/Traits 决定目标类型：
  - 高 `openness`、`explorer` Trait → 偏好探索类目标。
  - 高 `extraversion` → 社交需求优先级提升，拥挤惩罚降低。
  - 高 `neuroticism` → 选择更安全的互动点（库存足、危险低）。
- ActionPlanner 将目标转换为行动序列（Move → Interact → Wait…），每步行动可能再次引用人格/Traits 调整参数（运行速度、等待耐心、社交时长）。

## 觅食示例
```
score = w_dist * travelCost
      + w_crowd * crowdPenalty
      + w_stock * stockDeficit
      + traitBias
      + jitter
```
系数：
- `w_dist = 1.2 + 0.8 * conscientiousness - 0.8 * openness`
- `w_crowd = 0.3 + 1.5 * neuroticism - 1.0 * extraversion`
- `w_stock = 0.5 + 1.0 * neuroticism + 0.5 * conscientiousness - 0.4 * openness`
- `traitBias`：`gourmet` 额外偏好高品质食物点；`survivalist` 偏好安全/库存高的地点。
- `jitter`：`σ = 0.15 * openness - 0.1 * conscientiousness`

Planner 取最小 score 的候选节点，对应目标写入行动序列；DecisionSystem 负责确保逻辑与人格一致。

## Demo 画像（用于沙盒验证）
- **Explorer**：`O=0.8 C=0.4 E=0.6 A=0.5 N=0.3`  
  - Traits：`wanderlust`。行为：较高探索偏好，乐于尝试远处资源，轻微抖动。
- **Planner**：`O=0.3 C=0.8 E=0.4 A=0.6 N=0.5`  
  - Traits：`methodical`。行为：提前补给，偏好近处/高保障点，谨慎避拥挤。
- **Socializer**：`O=0.5 C=0.5 E=0.9 A=0.6 N=0.2`  
  - Traits：`people_person`。行为：社交需求权重高，更能接受拥挤环境。

## 实现约束
- 组件：
  - `AgentAttributes`：存储连续属性与衰减器。
  - `AgentNeeds`：上一帧需求强度、阈值、趋势。
  - `AgentPersonalityBig5`、`AgentTraits`。
- 运行时：
  - AttributeSystem 更新属性 → NeedSystem 使用配置表评估 → DecisionSystem 生成目标 → Planner/ActionSystem 执行。
  - 所有计算在模拟线程完成，前端只通过 Telemetry 观察结果。
- Telemetry：
  - `attributes[]`, `needs[]`, `traits[]`，Inspector 可展示人格与当前决策。

## 拓展方向
- 为更多需求定义映射（休息、娱乐、合作、风险、任务）。
- 支持可运行时调整的配置（热加载 JSON/Script），便于调参。
- 结合剧情/任务系统，让 Traits 决定可选行动或任务偏好。
- 引入记忆/关系数据，让人格影响感知与社交判断。
