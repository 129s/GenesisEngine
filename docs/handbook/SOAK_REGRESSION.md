# Soak 回归手册（方案 A：非门禁）

目标：把 “短/长 Soak” 作为**软指标**回归：可运行、可落盘、可对比、可追溯；但不作为 CI 的硬门禁（避免噪声/波动导致频繁红）。

## 1. 产物与目录约定

- Soak 报告（JSON）：包含资源经济、行为多样性、瓶颈候选等汇总指标。
- Soak 摘要（Markdown）：面向人工阅读的一页汇报。
- 默认输出目录：
  - `out/soak/`：单次 soak（见 `scripts/run_soak.ps1`）
  - `out/metrics/`：基准集合（见 `scripts/run_baseline_soak.ps1`）

建议把 `out/` 加入 `.gitignore`（当前仓库已这样做），并用压缩包/CI artifacts 或外部存储留档。

## 2. 推荐执行策略（A）

- PR/每次提交：只跑 Core 单测 + E2E（见 `scripts/run_regression_core.ps1`），不跑长 soak。
- 每周（手动）：跑“基准集合短/中等步数”生成对比产物（例如 800/5000/20000 steps）。
- 预发布/大改（手动）：跑 24h（墙钟）soak，留档并和最近一次同配置对比。

## 3. 本地执行

### 3.1 单世界 Soak（推荐入口）

短跑：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run_soak.ps1 -Preset short -Config RelWithDebInfo
```

强制使用 MSVC：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run_soak.ps1 -UseMsvc -Preset short -Config RelWithDebInfo
```

24h（墙钟停止；`--steps` 只作为上限）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run_soak.ps1 -Preset 24h -WallHours 24 -Config RelWithDebInfo
```

产物：`out/soak/soak_*.json` 与 `out/soak/summary_*.md`。

### 3.2 输出世界线（Worldline Chronicle，建议开启）

世界线是“宏观证据的编年史”，按窗口输出关键计数/指标，用于跨 seed/配置/版本对比与母题抽取；它不是逐帧日志。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run_soak.ps1 -UseMsvc -Preset short -Worldline -WindowSteps 5000
```

产物：`out/worldline/worldline_*.jsonl`。

### 3.2 基准集合 Soak（覆盖更广）

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run_baseline_soak.ps1 -Preset short -Seed 1337
```

产物：`out/metrics/soak_*.json` 与 `out/metrics/summary_*.md`（按 baseline 分组）。

## 4. 报告对比（不门禁）

使用 `scripts/compare_soak.ps1` 做两份 JSON 的关键字段对比（只报告差异，不做 fail/pass）。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\compare_soak.ps1 -Base out/soak/soak_x.json -Cand out/soak/soak_y.json
```

也可以对比两份世界线（按窗口粗对齐，输出差异最大的窗口；不做 pass/fail）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\compare_worldline.ps1 -A out/worldline/worldline_a.jsonl -B out/worldline/worldline_b.jsonl
```

## 5. CI 约定

- CI 默认仍以 `core-regression` 为主（单元/集成/E2E）。
- Soak 仅保留为手动触发的短跑（用于确认环境/产物格式），不作为门禁。
