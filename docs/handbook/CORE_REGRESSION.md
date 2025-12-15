# 内核回归闭环手册（Headless 优先）

目标：在不依赖 GUI 的前提下，固化“可构建、可回放、可断言、可对比”的内核回归闭环，作为后续涌现行为（Needs/Planner/Action/World）演进的安全网。

## 1. 推荐构建配置（Core Only）

```powershell
cmake -S . -B build_core -G Ninja `
  -DGENESISENGINE_ENABLE_TESTS=ON `
  -DGENESIS_BUILD_GUI=OFF `
  -DGENESIS_WITH_STYLE=OFF

cmake --build build_core --parallel
```

## 2. 运行核心测试套件

```powershell
ctest --test-dir build_core --output-on-failure
```

关注的回归信号（示例）：
- `genesis_runtime_tests`：事件队列、JSON 命令、快照 diff、涌现 smoke、多人回归
- `RuntimeShortSoak.*`：短时稳定性与可观测性基线（默认纳入 `genesis_runtime_tests`）
- `genesis_world_model_tests`：WorldDatabase 载入/路径规划等契约
- `genesis_world_tests`：资源系统与库存再生/消耗约束
- `genesis_worldgen_tests`：worldgen 配置/拓扑/布局/校验模块

## 3. Headless 回放：Runtime CLI 执行脚本

构建：

```powershell
cmake --build build_core --target genesis_runtime_cli
```

执行仓库内示例脚本（命令队列 + `waitForSuccess` 串联）：

```powershell
.\build_core\src\genesis-runtime-cli.exe run-script data\scripts\world_cycle.json --root . --max-steps 256 --after-steps 32
```

更多说明见：`docs/guides/headless_runtime_cli.md` 与 `docs/architecture/foundation/runtime-api.md`。

## 3.1 短 Soak：软指标报告（多样性 / 资源经济）

说明：Soak 报告用于版本间对比与调参，不作为 CI 硬门禁；CI 侧仍以单元/集成测试的“硬契约”与短 soak 的不变量断言为主。

```powershell
.\build_core\src\genesis-runtime-cli.exe soak data\world_multiagent --root . --steps 5000 --out out\soak_metrics.json
```

## 3.2 基准世界集合（建议用 worldgen 生成）

仓库提供一组 worldgen 基准配置（可复现、覆盖多样性/资源经济边界）：
- `data/worldgen/baselines/README.md`
- `data/worldgen/baselines/scarcity.toml`
- `data/worldgen/baselines/diversity.toml`
- `data/worldgen/baselines/abundance.toml`

一键生成并输出指标报告（写到 `out/`，不进版本库）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run_baseline_soak.ps1 -Steps 5000 -Seed 1337
```

短 soak（更快的本地验证，适合日常迭代）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run_baseline_soak.ps1 -Preset short -Seed 1337
```

## 4. 世界数据契约（避免旧术语干扰）

- v2 世界输入输出：目录结构 `world.json + map_{id}.json`
- 契约与字段：`docs/architecture/world/world-model.md`
- 迁移说明：`docs/handbook/WORLD_SCHEMA_MIGRATION.md`

建议：把仍引用 `WorldRegistry/LocationGraph/LocationId` 的文档视为历史资料，优先以 v2 契约为准。

## 5. 约定：什么是“回归闭环”

最小闭环（必须长期保持稳定）：
1) 能通过 `world.db.load` 装载 v2 世界；
2) 能通过 Telemetry 观察到 Needs / Planner 决策 / Action / Movement / Resource 变化；
3) 能通过命令队列注入（创建/移动/消耗/加载/保存/生成）并得到可断言的 `RuntimeEventReport`；
4) 单元/集成测试提供“失败即定位”的断言（而不是只跑不崩溃）。

扩展闭环（逐步增强）：
- 指标导出与对比：把 Telemetry/关键统计落盘，支持版本间 diff（趋势、分布、异常阈值）。
- 长时稳定性：可配置的 soak（例如 30min/2h/24h）与资源/需求/位置的安全约束检查。
