# Backlog · 噪声地图 MVP

> 2025-10 更新：噪声生成 CLI / 脚本已退役，条目仅保留历史记录；现行世界生成请改用 Worldgen 管线。

> 与 `docs/roadmap/MVP_NOISE_MAP.md` 对应的工作拆分，按依赖顺序列出后续动作。

## Phase 1 · 生成器基础
- [x] 实现 `NoiseGridGenerator`：封装 `seed,width,height,threshold` → 二值网格，保留相同 seed 决定性。
- [x] 将网格映射为 `LocationGraph`：节点带 `terrain` 标签，相邻可通行单元生成无向边，`cost=1.0`。
- [x] 采样资源点：在 `Soil` 节点上生成 `ResourceSpawn{Food,capacity,ratePerStep}`，确保密度/随机性可配置。
- [x] 导出 JSON：写入 `data/world/generated/noise_mvp.json` 与匹配的布局数据。
- [x] 单元测试：覆盖 seed 决定性、地形占比统计、图连通性与资源生成约束。

> 结果（历史记录）：新增 `NoiseGridGenerator`（`include/src/world/generation`），产出带 `terrain` 标签的 `LocationGraph` 与布局元数据；原单元测试文件 `tests/test_noise_grid_generator.cpp` 覆盖 Determinism/Distribution/Edges/Spawns/Layouts（已在 2025-10 移除，由 Worldgen 测试接手保障）。

## Phase 2 · 命令与自动化
- [x] 在 `scripts/` 增加命令（PowerShell/Python）触发生成流程，支持参数 `--seed/--width/--height/--threshold`。
- [x] 将生成产物加入构建/打包（CMake 或后处理）流程，保证默认 Demo 可直接使用。
- [x] 更新文档与示例命令，说明如何生成/刷新噪声地图数据。

> 结果（历史记录）：新增 `src/tools/NoiseGenMain.cpp` → `genesis-noise-generator` CLI，同步提供 `scripts/generate_noise_world.ps1` 包装脚本；CMake 自定义目标 `generate_noise_world` 默认写入 `data/world/generated/noise_mvp.json` 与 `noise_mvp_layout.json`（上述资产在 2025-10 后停止维护，现已从仓库移除）。

## Phase 3 · 运行时接入
- [x] 引擎提供显式 `loadWorldFromFile` 接口加载噪声世界，可通过脚本/配置传入路径。
- [x] 确认资源系统沿用 `ratePerStep` 的刷新逻辑，处理生成图的新资源点。
- [x] 设定代理初始落点：加载时挑选可通行节点并放置起始代理。
- [x] 集成测试：通过运行时 API 验证地图加载成功且资源刷新无异常。

> 结果：引擎提供显式加载接口并在加载后挑选可通行节点投放代理；`tests/test_runtime.cpp` 验证噪声世界加载与资源刷新闭环。

## Phase 4 · NPC 生命周期验证
- [x] 准备脚本：在沙盒 GUI 中执行固定步骤序列（`step/pause/resume`）观察行为。
- [x] 记录指标：确认 `MoveTo`→`ConsumeResource` 循环出现，`Hunger` 需求下降后回升。
- [x] 添加调试/日志：必要时扩展 Telemetry 输出以便观察行为闭环。

> 结果：沙盒 GUI 可复现循环，并在 MapView/Telemetry 中展示 `MoveTo/ConsumeResource` 与 Hunger 数值；`tests/test_runtime.cpp` 增加噪声世界闭环断言（移动、消费与饥饿下降）。

## Phase 5 · 可视化支持
- [x] 基于生成的布局数据渲染地图视图，显示 `A/M/C/F` 状态。
- [x] 补充文档与截图/录屏指引，便于快速验证渲染效果。

> 结果：沙盒 GUI MapView 提供 `Legend` 与行为摘要，支持一键生成并持续运行；文档补充了截屏/录屏指引。

### 验收回归清单
- [ ] 相同 seed 下生成文件一致，不同 seed 产生差异。
- [ ] 地形占比与阈值预期相符（阈值 0.5 ≈ 1:1）。
- [ ] `ResourceSpawn.current` 随时间趋近 `capacity`，消费时正确下降。
- [ ] NPC 在若干步内完成一次 `MoveTo` + `ConsumeResource` 循环。
- [ ] GUI 渲染对齐生成布局，可观察状态随时间变化。

