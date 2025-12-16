# Agent 模式 v0（已落地规范）

本文档以当前仓库代码为准，描述 **已经落地** 的 Agent 运行模型（ECS 组件 + 系统职责 + Tick 顺序 + 决策/行动语义）。  
与设想/路线图类内容分离：未来扩展请写入 `docs/architecture/proposals/`。

## 范围与非目标
- 范围：Headless Runtime/Core Simulation（不含 GUI 表现层）。
- 非目标：人格 Big5、Traits、属性→需求的数据驱动映射、记忆/关系/叙事系统（这些属于提案，见 proposal）。

## 数据模型（ECS 组件）
- **人格（Big5）**
  - `genesis::agents::AgentPersonalityBig5`：OCEAN（0~1）人格向量（`include/genesis/agents/Personality.hpp`）。
  - 生成方式：Agent 创建时若未显式指定，则按 `entityId + mapId` 的确定性 RNG 生成（可复现、无外部脚本介入）。
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
  - `ActionType`：`MoveToInteraction / ConsumeResource / TakeResource / ProduceResource`（同上）。
- **随身资源**
  - `genesis::agents::components::CarriedResources`：按资源类型分槽的随身库存（`include/genesis/agents/CarriedResources.hpp`）。

## 系统职责
- **NeedSystem**：按 `decayPerSecond * deltaSeconds` 推进需求值并产出采样（`src/agents/NeedSystem.cpp`）。
- **NeedSatisfier（当前的“选点/决策”实现）**
  - 读取 Need 状态，计算 urgency，并在可达交互点中选择“最合适”的目标资源点；
  - 写入 `PlannerDecision`，并向 `ActionExecutor` 申请 `ConsumeResource`（`src/agents/NeedSatisfier.cpp`）。
- **ActionExecutor**
  - 执行行动队列；必要时插入 `MoveToInteraction`；
  - `ConsumeResource/TakeResource`：从 `ResourceSystem` 消耗资源，写 Telemetry Attempt；
  - `ProduceResource`：解析工坊配方，消耗输入并向工坊库存产出；
  - 若 `ConsumeResource` 因缺货失败，会调用 `ProductionPlanner` 自动补链（`src/agents/ActionSystem.cpp`、`src/agents/ProductionPlanner.cpp`）。
- **Movement2DSystem**
  - 处理 `MovementIntent2D`，在地图内直线移动；跨图时走 Portal（`src/simulation/Movement2DSystem.cpp`）。
- **ResourceSystem**
  - 维护资源点库存：源点 regen、配置的腐败 decay、工坊库存变化（`src/world/system/ResourceSystem.cpp`）。

## Tick 顺序（调度约束）
当前调度顺序为（见 `src/simulation/Scheduler.cpp`）：
1) `NeedSystem.update`：需求随时间上升  
2) `NeedSatisfier.update`：选择目标并下发动作（若已有队列/移动则跳过）  
3) `ActionExecutor.update`：执行 Consume/Take/Produce 与队列推进  
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
  - `decisionJitter`：每次选点会注入小幅确定性噪声（由 `stepIndex` 与 `entityId` 导出），用于打破“全体收敛到同一最优点”的稳态。
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
