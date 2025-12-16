# Proposal · Agent Behavior & Personality（Big5 / Traits / 数据驱动 Needs）

状态：**Proposal（部分落地）**。  
本文件描述的是“期望的设计方向”，不代表当前代码行为。已落地规范请看：
- `docs/architecture/agents/agent-model-v0.md`
- `docs/architecture/agents/agent-implementation-status.md`

---

## 概述
- 目标：以数据驱动方式串联 `属性 → 需求 → 动机 → 行动`，为后续多需求/多系统扩展奠定基础。
- 概念：
  - `Attributes`：连续状态（health / hunger / sanity / energy 等）。
  - `Needs`：根据属性、人格、Trait 与外部刺激计算出的欲求强度。
  - `Personality`：大五人格（OCEAN，0~1），组件 `AgentPersonalityBig5 { openness, conscientiousness, extraversion, agreeableness, neuroticism }`。
  - `Traits`：离散标签（`night_owl`、`gourmet`…），改变阈值、偏好或解锁特殊行动。
- 行为链路（目标态）：属性更新 → NeedSystem 评估需求 → Planner 选取目标 → ActionExecutor / MovementSystem 执行 → Telemetry 记录结果。

## 已落地子集（当前）
- Big5 作为 Agent 组件已落地，并被 `NeedSatisfier` 用于：
  - 距离/稀缺/拥挤惩罚的权重缩放；
  - 准备阈值与目标切换“粘性”的缩放；
  - 确定性 jitter（由 stepIndex 与 entityId 导出）以打破全体收敛。
- 仍未落地：Attributes 层、Traits、数据驱动映射表与曲线/jitter 参数化。

## 属性与需求映射（目标态）
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
- NeedSystem 按帧读取属性与人格，计算欲求强度（示意）：
```
intensity = sigmoid(baseCurve, Σ attrWeight * attributeValue)
            + Σ personalityWeight * OCEAN
            + Σ traitModifier
            + jitter(Personality, seed)
```

## 动机生成与行动执行（目标态）
- Planner 读取 Need 排序，结合人格/Traits 决定目标 Scene/Interactive：
  - 高 `openness` + `wanderlust`：偏好探索型目标或远距离资源。
  - 高 `extraversion`：对拥挤的资源点容忍度更高。
  - 高 `neuroticism`：优先选择库存充足或风险低的地点。
- Planner 输出决策，写入行动序列；ActionExecutor 执行并反馈结果。

## 评分示例：觅食（目标态）
```
score = w_dist * travelCost
      + w_crowd * crowdPenalty
      + w_stock * stockDeficit
      + traitBias
      + jitter
```
示例系数（仅示意）：
- `w_dist = 1.2 + 0.8 * conscientiousness - 0.8 * openness`
- `w_crowd = 0.3 + 1.5 * neuroticism - 1.0 * extraversion`
- `w_stock = 0.5 + 1.0 * neuroticism + 0.5 * conscientiousness - 0.4 * openness`

## Demo 画像（用于沙盒验证）
- **Explorer**：`O=0.8 C=0.4 E=0.6 A=0.5 N=0.3`（Traits：`wanderlust`）
- **Planner**：`O=0.3 C=0.8 E=0.4 A=0.6 N=0.5`（Traits：`methodical`）
- **Socializer**：`O=0.5 C=0.5 E=0.9 A=0.6 N=0.2`（Traits：`people_person`）

## 实现约束（落地前提）
- 需要引入组件：`AgentAttributes`、`AgentPersonalityBig5`、`AgentTraits`。
- 需要补齐：配置表加载、数据驱动映射、Telemetry 扩展、GUI Inspector 展示。
