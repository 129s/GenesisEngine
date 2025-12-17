# Worldline Chronicle（世界线编年史）· v0

> 目的：提供一种“比日志更宏观、比指标更可读”的证据载体，用于跨 seed/配置/版本对比、母题（Motif）抽取与人工分析。  
> Worldline 是 **JSONL**（一行一个 JSON 对象），方便流式写入与增量处理。

## 1. 生成方式

当前由 `genesis-runtime-cli soak` 生成：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run_soak.ps1 -UseMsvc -Preset short -Worldline -WindowSteps 5000
```

输出：`out/worldline/worldline_*.jsonl`

## 2. 文件结构（按行）

### 2.1 Meta 行（第 1 行）

`kind = runtime_worldline_meta`

包含：
- `schema_version`：Worldline 自身 schema 版本（独立于 Telemetry schema）
- 备注：当前实现已升级到 `schema_version=2`（`stockoutShare*` 口径更新，见下）。
- `worldFolder`、`windowSteps`、`stepsRequested`、`wallSecondsLimit?`
- `agentCountRequested`、`agentCountInitial`
- `worldVersion`、`lastSeed?`
- `telemetrySchemaVersion`
- `atlas{schema_version, world_version}`

### 2.2 Window 行（后续多行）

`kind = runtime_worldline_window`

按窗口聚合（窗口长度=`windowSteps`，最后一窗可能不足）：
- `index`：窗口序号（从 0）
- `stepStart`/`stepEnd`：窗口覆盖的“采样步”范围（从 0，包含 bootstrap 快照）
- `metrics{...}`：窗口级宏观指标（用于变点/阶段识别）
- `counts{...}`：窗口级证据计数（用于分布对比与母题匹配）

当前 v0 输出的 `metrics` 包括：
- `criticalNeedRate`：窗口内 need 样本中 critical 的比例
- `stockoutShareAny/Sources/Workshops`：窗口内“资源交互点缺货占比”的平均值（总/来源/工坊），计算方式：对每一步求 `(#current==0)/(#total)`，再在窗口内取平均。
- `stepsWithAnyConsumption/Regen/Decay`：窗口内出现对应事件的步数
- `actionEntropyBits`：行动类型分布熵（bit）
- `plannerTargetEntropyBits`：Planner 目标交互点分布熵（bit）
- `plannerTravelCostMean`：Planner travelCost 均值
- `resourceAttempts/resourceFailed`：资源尝试与失败数
- `productionAttempts/productionFailed`：生产尝试与失败数

`counts` 当前包括：
- `actionTypes{ name -> count }`
- `plannerTargets{ interactionId(string) -> count }`
- `resourceFailureReasons{ reason -> count }`
- `productionFailureReasons{ reason -> count }`

## 3. 对比用途（v0）

`scripts/compare_worldline.ps1` 做“窗口粗对齐”的差异定位：
- 计算 actionTypes / plannerTargets 的 Jensen–Shannon divergence（bit）
- 输出差异最大的 Top 窗口，并附带关键指标 delta

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\compare_worldline.ps1 -A a.jsonl -B b.jsonl
```

## 4. 与母题库的关系（下一步）

Worldline Window 是母题匹配的基础证据层：
- 用 `D(w,m)`（母题在世界 w 的覆盖窗口数/占比）作为主特征；
- 对候选母题做“正交性/冗余”检查（共现、互信息、相关、条件冗余）；
- 保留指针回到更底层证据（Telemetry/事件报告）做人工归因。

母题治理与统计检查详见 `docs/architecture/meta/motif-governance.md`。

### 4.1 离线母题挖掘（v0）

为了尽量避免“预设感”，母题候选以 **worldline 窗口数值特征**为输入，自动挖掘“滞后相关的因果边”与“对齐高分链条”母题（A→B→C，基于 z-score 对齐分数的分位阈值）。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\analyze_worldline_motifs.ps1 -InputPath out/worldline -StateQuantile 0.9
```

输出：`out/worldline/analysis/motifs_*.json`
