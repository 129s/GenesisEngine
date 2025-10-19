# 噪声世界数据生成指引

> 面向 Phase 2：命令与自动化的日常使用说明。

## 一键生成（PowerShell 脚本）

```powershell
pwsh ./scripts/generate_noise_world.ps1 -Seed 1337 `
    -Width 64 -Height 64 -Threshold 0.5 `
    -SoilDensity 0.05 -Capacity 24 -Rate 3 `
    -WorldPath data/world/generated/noise_mvp.json `
    -LayoutPath data/world/generated/noise_mvp_layout.json
```

- 首次执行会自动调用 `cmake -S . -B build` 并构建 `genesis-noise-generator` 可执行文件。
- 参数使用说明：
  - `Seed` **必填**，同一 seed 保证生成结果一致。
  - `Width`/`Height` 控制噪声网格尺寸（默认 64x64）。
  - `Threshold` 为噪声阈值，决定石头/土壤的分界。
  - `SoilDensity` 控制在可通行单元上采样资源点的概率。
  - `Capacity`/`Rate` 对应 `ResourceSpawn` 的容量与固定刷新速率。
  - `WorldPath`、`LayoutPath` 可定制输出位置；默认写入 `data/world/generated/`。
- 追加 `-Reconfigure` 或 `-Rebuild` 可触发重新配置/编译。

## 直接使用 CLI

```bash
cmake --build build --target genesis_noise_generator
build/src/genesis-noise-generator.exe --seed=1337 --width=64 --height=64 \
    --threshold=0.5 --soil-density=0.05 --capacity=24 --rate=3 \
    --world=data/world/generated/noise_mvp.json \
    --layout=data/world/generated/noise_mvp_layout.json
```

- 命令行参数与脚本一致，均支持 `key=value` 形式。
- 运行目录建议置于仓库根目录，方便直接写入 `data/`。

## Sandbox CLI 快速运行

```bash
cmake --build build --target genesis_sandbox_cli
build/src/genesis-sandbox-cli --generate-noise --auto-run --no-clear --fps 0
```

- `--generate-noise` 使用默认参数即时生成噪声世界（可通过 `--noise-*` 系列参数调整种子、尺寸与资源配置）。
- `--auto-run` 连续推进模拟，适合观察完整 NPC 生命周期；需要停止时使用 `Ctrl+C` 或在 `--commands` 中追加 `quit`。
- `--no-clear`/`--fps 0` 便于录像或快速回放，可按需移除以降低刷新频率。
- CLI 会自动读取生成的布局并在 Legend 行标注 `A/M/C/F` 符号。
## CMake 集成目标

```bash
cmake --build build --target generate_noise_world
```

- 依赖 `genesis_noise_generator`，默认使用 `seed=1337` 输出到 `data/world/generated/`。
- 可作为打包或 CI 步骤，保证示例数据始终同步。

## 输出内容

- `data/world/generated/noise_mvp.json`：`LocationGraph` JSON（节点含 `terrain` 标签，边为可通行关系，资源点为 `Food`）。
- `data/world/generated/noise_mvp_layout.json`：配套 ASCII 布局，`sandbox_cli` 可通过 `--layout` 参数加载可视化。

生成后可使用 `scripts/run_sandbox_cli.ps1` 并指定新布局观察资源刷新与 NPC 行为。

## 运行时加载

- 运行时不再自带演示世界，需要显式调用 `Runtime::loadWorldFromFile(path)`（或在 `RuntimeConfig::initialWorldPath` 中配置）加载生成结果。
- `scripts/run_sandbox_cli.ps1` 与 `--generate-noise` 参数会将生成的世界路径写入运行时配置，也可在 CLI/GUI 的“World Generation”面板手动选择并加载。
- 加载成功后 Demo 代理会在图中选取可通行节点作为出生点，资源刷新逻辑保持不变。

## 验证 NPC 闭环

```powershell
pwsh ./scripts/run_sandbox_cli.ps1 -UseGeneratedWorld `
    -Commands "step 5;pause;resume;step 40;quit" -NoClear
```

- 该脚本会临时设置 `GENESIS_WORLD_PATH` 指向 `data/world/generated/noise_mvp.json` 并使用匹配的布局。
- CLI 输出的 `Actions:` 区域可观察到 `MoveTo` → `ConsumeResource` 的循环，同时 `Needs:` 显示 Hunger 数值在消费后回落。
- 建议加上 `-NoClear` 便于截屏或录屏，终端顶部的 Legend 行会标注 `A/M/C/F` 等符号含义，可直接用于文档截图。
- 自动化回归可参考 `tests/test_runtime.cpp` 中的 `AgentCompletesConsumeCycleOnNoiseWorld`，通过 Telemetry 验证移动与消费步骤及饥饿下降。

