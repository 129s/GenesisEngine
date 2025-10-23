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

## Sandbox GUI 快速检查

```bash
cmake --build build --target genesis_sandbox_gui
./build/src/genesis-sandbox-gui.exe
```

- 启动后在 World Generation 面板提交 `world.generate` / `world.load` 命令即可加载新增数据（详见 `docs/guides/sandbox_gui_smoke.md`）。
- Map View/Inspector 会实时展示资源刷新、代理行为与 Telemetry 指标，可替代历史 CLI 的 ASCII 观察。

## CMake 集成目标

```bash
cmake --build build --target generate_noise_world
```

- 依赖 `genesis_noise_generator`，默认使用 `seed=1337` 输出到 `data/world/generated/`。
- 可作为打包或 CI 步骤，保证示例数据始终同步。

## 输出内容

- `data/world/generated/noise_mvp.json`：`LocationGraph` JSON（节点含 `terrain` 标签，边为可通行关系，资源点为 `Food`）。
- `data/world/generated/noise_mvp_layout.json`：配套布局数据（历史 ASCII 渲染使用）；可留作离线调试或自定义可视化之用。

## 运行时加载

- 运行时不再自带演示世界，需要显式调用 `Runtime::loadWorldFromFile(path)`（或在 `RuntimeConfig::initialWorldPath` 中配置）加载生成结果。
- 可通过 GUI 的 World Generation 面板选择生成好的世界并加载，或在 `RuntimeConfig::initialWorldPath` 里设置路径实现自动加载。
- 加载成功后 Demo 代理会在图中选取可通行节点作为出生点，资源刷新逻辑保持不变。

## 验证 NPC 闭环

在 GUI 中可通过命令队列加载生成的世界并观察 Map View/Telemetry 中的 `MoveTo → ConsumeResource` 循环；若需程序化验证，可参考 `tests/test_runtime.cpp` 中的 `AgentCompletesConsumeCycleOnNoiseWorld`（Telemetry 断言移动、消费与饥饿下降）。

