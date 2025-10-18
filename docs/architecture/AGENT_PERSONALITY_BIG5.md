# Agent Behavior & Personality（属性→需求→动机→行动）

## 概述
- 目标：以数据驱动方式串联 `属性 → 需求 → 动机 → 行动`，为后续多需求/多系统扩展奠定基础。
- 概念：
  - `Attributes`：连续状态（health / hunger / sanity / energy 等）。
  - `Needs`：根据属性、人格、Trait 与外部刺激计算出的欲求强度。
  - `Personality`：大五人格（OCEAN，0~1），组件 `AgentPersonalityBig5 { openness, conscientiousness, extraversion, agreeableness, neuroticism }`。
  - `Traits`：离散标签（`night_owl`、`gourmet`…），改变阈值、偏好或解锁特殊行动。
- 行为链路：属性更新 → NeedSystem 评估需求 → Planner 选取目标 Scene/Interactive → ActionExecutor / MovementSystem 执行 → Telemetry 记录结果。

## 属性与需求映射
- 每个 Need 通过数据定义（JSON/YAML/表格不限）：
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
- NeedSystem 按帧读取属性与人格，计算欲求强度：
```
intensity = sigmoid(baseCurve, Σ attrWeight * attributeValue)
            + Σ personalityWeight * OCEAN
            + Σ traitModifier
            + jitter(Personality, seed)
```
- 阈值（`satisfiedThreshold`、`prepareMargin` 等）同样通过数据定义，并允许人格/Traits 调节：
  - `threshold = base + 0.6 * conscientiousness + 0.6 * neuroticism`
  - `prepareMargin = baseMargin + traitBonus`

## 动机生成与行动执行
- Planner（例如 `HungerPlanner`）读取 Need 排序，结合人格/Traits 决定目标 Scene/Interactive：
  - 高 `openness` + `wanderlust` Trait：偏好探索型目标或远距离资源。
  - 高 `extraversion`：对拥挤的资源点容忍度更高。
  - 高 `neuroticism`：优先选择库存充足或风险低的地点。
- Planner 输出 `HungerDecision`（目标、旅行成本、评分），写入 `MovementIntent` 或 `ActionQueue`。
- `ActionExecutor` 负责将目标拆解为 Move / Consume / Portal 等任务，并在执行时再次参考人格（如调整速度、等待容忍度）。

## 评分示例：觅食
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
