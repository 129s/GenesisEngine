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

