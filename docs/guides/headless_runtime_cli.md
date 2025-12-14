# Headless Runtime CLI（Windows 优先）

目的：在不启用 GUI 的情况下，跑通 Runtime 的 JSON 命令脚本（含 `waitForSuccess` 串联），用于回归/重放与最小闭环验证。

## 构建

```powershell
cmake -S . -B build_headless -G Ninja -DGENESIS_BUILD_GUI=OFF
cmake --build build_headless --target genesis_runtime_cli
```

产物：`build_headless/src/genesis-runtime-cli.exe`

## 运行脚本

仓库内示例脚本：`data/scripts/world_cycle.json`（包含 `world.db.load` → `resource.consume` → `world.db.save`）。

从仓库根目录执行（推荐显式指定 `--root .`，保证脚本内相对路径可解析）：

```powershell
.\build_headless\src\genesis-runtime-cli.exe run-script data\scripts\world_cycle.json --root . --max-steps 64
```

常用选项：
- `--world <folder>`：启动时先加载一次世界（可选；脚本也可自行 `world.db.load`）
- `--events-out <file>`：把执行过的 `RuntimeEventReport` 列表写到 JSON 文件
- `--after-steps <n>`：脚本执行完后额外推进 n 步（用于观察非事件驱动的演化）

## 短 Soak 指标报告（软指标，不做硬门禁）

目的：以 “多样性 + 资源经济” 为主做可对比的度量输出，用于版本间对比与调参，不作为 CI 的硬断言。

示例（输出到 stdout）：

```powershell
.\build_headless\src\genesis-runtime-cli.exe soak data\world_multiagent --root . --steps 5000 --agents 12
```

示例（写入 JSON 文件）：

```powershell
.\build_headless\src\genesis-runtime-cli.exe soak data\world_multiagent --root . --steps 5000 --out out\soak_metrics.json
```

输出字段要点：
- `diversity.actionTypes`：按 `telemetry.actions[].currentAction` 聚合的计数与熵（bits）
- `diversity.plannerTargets`：按 `telemetry.plannerDecisions[].target` 聚合的计数与熵（bits）
- `resourceEconomy`：每个资源点的 min/max/start/end、累计消耗/产出（由运行时统计并写入 `telemetry.resources[].consumed/produced`）、stockout 统计；其中 `resources[].type` 为字符串（`Food/Water/Social`），并保留 `resources[].typeId` 便于脚本化处理
- `summary`：面向人工汇报的摘要（工坊产出 vs 自然再生、stockout、以及基于“热度+缺货”的瓶颈候选 `bottlenecksTop`）
