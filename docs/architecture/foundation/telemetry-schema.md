# Telemetry Schema（TickTelemetry v3）

本文档定义运行时对外暴露的 **TickTelemetry** 数据结构（GUI/工具链消费的只读快照内容之一），并提供最小的版本演进记录，作为“可观测性闭环”的协议基线。

范围：
- 覆盖 `genesis::telemetry::TickTelemetry` 及其子结构（Agents/Needs/Planner/Actions/Resources）。
- 关注字段语义与版本兼容策略；不规定具体序列化格式（JSON/二进制等可后续扩展）。

非范围：
- Soak 报告（软指标汇总 JSON/Markdown）属于分析产物，字段可按需要演进；其生成逻辑以 `genesis-runtime-cli soak` 为准。

## 1. 版本化策略

- `TickTelemetry.schema_version` 用于 **前端/工具链解析兼容性**：
  - 前端应在启动/首帧读取时校验 `schema_version`，遇到未知版本应降级或拒绝解析（避免“静默错读”）。
- 版本提升时必须同步更新：
  - `docs/architecture/foundation/runtime-api.md`
  - 本文档的“变更记录”
- 推荐约定（当前仓库采用偏保守策略）：
  - **新增字段也提升版本**（即使是可选字段），用版本号显式驱动消费端适配。

## 2. TickTelemetry（v3）字段字典

代码定义：`include/genesis/telemetry/TelemetryBuffer.hpp`

### 2.1 顶层字段

- `schema_version:uint32`：Telemetry 协议版本（当前为 3）。
- `step:uint64`：模拟步号（离散时间）。
- `stepSeconds:float`：单步的时间长度（秒），由 `SimulationClock::stepDuration()` 推导；用于把“每步统计”换算为“每秒速率”。
- `agents:AgentSnapshot[]`：代理位置与身份摘要。
- `resources:ResourceSnapshot[]`：资源点库存与本步变化量（关键经济信号）。
- `needs:NeedSnapshot[]`：需求强度（当前主要用于 Hunger 等）。
- `plannerDecisions:PlannerSnapshot[]`：本步规划结果（目标与评分）。
- `actions:ActionSnapshot[]`：行动执行状态（队列/当前动作/目标/消耗意图）。
- `movements:MovementSnapshot[]`：移动细节（当前实现可能为空；用于未来的可视化插值/轨迹）。

> 采集时机：每步执行完系统 `tick(...)` 后采集（见 `src/engine/core/Engine.cpp`），因此 `current/capacity` 等为“本步结束时”的状态快照；`consumed/produced/decayed` 为“本步内变化量”。

### 2.2 AgentSnapshot

- `entityId:uint32`：ECS 实体 ID（运行期内稳定；跨次运行不保证一致）。
- `name:string`：可读名（用于调试与 UI 展示）。
- `mapId:uint32`：所在 Map。
- `position:{x:float,y:float}`：地图内世界坐标（直线移动语义）。

### 2.3 NeedSnapshot

- `entityId:uint32`：对应代理实体 ID。
- `needName:string`：需求名（例如 `Hunger`）。
- `value:float`：需求强度（越高越紧迫；具体范围由 Need 实现决定）。
- `critical:bool`：是否处于临界（用于 UI 高亮/告警）。

### 2.4 PlannerSnapshot

- `entityId:uint32`：对应代理实体 ID。
- `target:InteractionId(uint32)`：规划选中的目标交互点（0 表示无目标/未规划）。
- `travelCost:float`：到目标的代价估计（单位由实现决定；目前主要用于相对排序）。
- `score:float`：综合评分（Utility/启发式分数；用于解释“为什么选它”）。

### 2.5 ActionSnapshot

- `entityId:uint32`：对应代理实体 ID。
- `currentAction:string`：当前动作类型（例如 `MoveToInteraction/ConsumeResource/TakeResource/ProduceResource/Idle` 等）。
- `queueLength:uint32`：行动队列长度。
- `target:InteractionId(uint32)`：当前动作目标（如移动/消耗/生产的交互点）。
- `speed:float`：移动速度（仅在移动相关动作时有意义）。
- `resource:ResourceType(enum)`：与当前动作相关的资源类型（例如消费/取货/生产的资源）。
- `amount:uint32`：与当前动作相关的数量（例如本次想消耗/取货的单位数）。
- `reliefPerUnit:float`：每单位资源对需求的缓解值（用于解释“为何要拿多少”）。

### 2.6 ResourceSnapshot

- `interactionId:uint32`：资源点对应的交互点 ID（可与 WorldAtlas 的 Interaction 对齐）。
- `mapId:uint32`：资源点所在 Map（若采集时未知，运行时会尝试回填）。
- `name:string`：资源点可读名（通常来自 Interaction 名称）。
- `type:ResourceType(enum)`：资源类型（如 `Water/Food/Social/Ore/Tool`）。
- `current:uint32`：库存现值（本步结束时）。
- `capacity:uint32`：库存上限。

本步变化量（用于区间统计/对比，不作为世界状态的唯一来源）：
- `consumed:uint32`：本步被代理从该资源点取走/消耗的数量（由 ResourceSystem 在执行动作时累加）。
- `produced:uint32`：本步产生的数量（包括 Source 的再生，以及工坊 `ProduceResource` 产出）。
- `decayed:uint32`：本步腐败/衰减的数量（由 ResourceSystem 在资源 tick 中扣减；当前语义是 **先衰减后再生**）。

> 提示：工坊资源点可能在初始化时 `current=0`，因此 “stockout” 在工坊上是常态；分析时应优先用 `produced/consumed/decayed` 与工坊成功率类指标判断是否真正“卡死”。

### 2.7 MovementSnapshot

当前结构定义存在，但采集可能尚未接入（取决于运行时实现与前端需求）：
- `entityId:uint32`
- `mapId:uint32`
- `position:{x:float,y:float}`
- `targetMapId:uint32`
- `target:{x:float,y:float}`
- `speed:float`

## 3. 变更记录（Changelog）

### v3
- `resources[].decayed`：新增资源腐败/衰减的本步变化量字段，用于度量“浪费/周期性压力”（生态链基线已使用）。

### v2（历史基线）
- v2 作为早期稳定版本，字段集合以 v3 的子集为主；差异以实际代码/提交为准。

