# Eventline Schema（对象演化事实流）· v1

> 目的：以“稀疏事实（sparse facts）”记录**单个观测对象（默认一个 agent）**在世界中的演化轨迹，供离线 lens（可组合观测算子）做对比/分析。  
> 原则：**eventline 只写观测到的事实，不在 runtime 内做 outcome 推断**；任何推断/归因留给离线脚本。

## 1. 生成方式

通过 `genesis-runtime-cli soak`：

```powershell
build_regression_core\src\genesis-runtime-cli.exe soak `
  --root . `
  --steps 5000 `
  --agents 12 `
  --worldgen-config data\worldgen\baselines\tool_feedback_v3.toml `
  --seed 1000 `
  --eventline-out out\eventline\eventline_seed1000_agent0.jsonl `
  --eventline-agent-index 0 `
  --quiet
```

批量跑与自动生成报告见：`scripts/run_observability_batch.ps1`。

## 2. 文件结构（JSONL，一行一个对象）

### 2.1 Meta 行（第 1 行）

`kind = runtime_eventline_meta`

必备字段（v1）：
- `schema_version`：eventline schema 版本（当前为 1）
- `notes`：说明（强调 “sparse facts / no inference”）
- `agentEntityId`：被观测 agent 的 `entityId`
- `agentIndexSortedByEntityId`：观测对象在“按 entityId 排序的 agent 列表”中的 index
- `worldSeed` / `worldgenConfig`
- `stepsRequested`
- `telemetrySchemaVersion`

### 2.2 Event 行（后续多行）

`kind = runtime_eventline_event`

字段：
- `step`：事件发生时的 step（从 0）
- `entityId`：事件归属实体（当前为被观测 agent 的 `entityId`）
- `type`：事件类型（见下）
- `payload`：事件载荷（append-only；字段可增不删）

## 3. 事件类型（v1）

> 注意：payload 字段均为“当时可直接观测到的信息”。离线可在此基础上派生统计、变点、对比等。

### 3.1 `initial_state`

首次可观测快照（用于离线重建/对比的锚点）：
- `mapId`
- `plannerTarget`
- `action`
- `needs{ needName -> { value, critical } }`

### 3.2 `map_change`

跨图移动：
- `from` / `to`

### 3.3 `planner_target_change`

Planner 目标切换：
- `from` / `to`
- `toIsAgentTarget`：是否为“以 agent 为目标”（若适用）
- `travelCost` / `score`

### 3.4 `action_change`

动作切换：
- `from` / `to`
- `queueLength`
- 可选：`targetEntityId`（当 `to=SocializeWithAgent`）
- 可选：`target` / `resourceType` / `amount`（部分资源相关动作）

### 3.5 `need_critical_transition`

某个 need 的 critical 状态变化：
- `need`
- `from` / `to`（bool）
- `value`

### 3.6 `need_delta`

某个 need 的数值变化（只在变化发生时写一条）：
- `need`
- `delta`
- `from` / `to`
- `critical`

### 3.7 `resource_attempt`

资源交互尝试（取/吃/拿等）：
- `action`
- `interactionId`
- `resourceType`
- `wantedUnits` / `obtainedUnits`
- `failureReason`（空表示无失败原因；但离线是否视为成功取决于 `obtainedUnits`）
- `recoveryPlanned`

### 3.8 `workshop_attempt`

生产/工坊尝试：
- `interactionId`
- `outputType`
- `wantedUnits` / `producedUnits`
- `failureReason`

### 3.9 `social_attempt_start`

社交开始：
- `partnerEntityId`
- `intendedRelief`
- `socialValue`

### 3.10 `social_attempt_end`

社交结束（只记录观测到的 Social need 前后值）：
- `partnerEntityId`
- `socialStart` / `socialEnd`
- `socialDelta`
- `intendedRelief`

## 4. 版本策略

- `schema_version` 单调递增。
- payload 字段**只增不删**（append-only），旧字段语义不变。
- 如果出现语义变更（口径/定义变化），必须：
  - 升级 `schema_version`
  - 在 `notes` 里注明变更点

