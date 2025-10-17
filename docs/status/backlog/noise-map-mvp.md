# Backlog · 噪声地图 MVP

> 与 `docs/roadmap/MVP_NOISE_MAP.md` 对应的工作拆分，按依赖顺序列出后续动作。

## Phase 1 · 生成器基础
- [ ] 实现 `NoiseGridGenerator`：封装 `seed,width,height,threshold` → 二值网格，保留相同 seed 决定性。
- [ ] 将网格映射为 `LocationGraph`：节点带 `terrain` 标签，相邻可通行单元生成无向边，`cost=1.0`。
- [ ] 采样资源点：在 `Soil` 节点上生成 `ResourceSpawn{Food,capacity,ratePerStep}`，确保密度/随机性可配置。
- [ ] 导出 JSON：写入 `data/world/generated/noise_mvp.json` 与匹配的布局 `data/ascii_layout.json`。
- [ ] 单元测试：覆盖 seed 决定性、地形占比统计、图连通性与资源生成约束。

## Phase 2 · 命令与自动化
- [ ] 在 `scripts/` 增加命令（PowerShell/Python）触发生成流程，支持参数 `--seed/--width/--height/--threshold`。
- [ ] 将生成产物加入构建/打包（CMake 或后处理）流程，保证默认 Demo 可直接使用。
- [ ] 更新文档与示例命令，说明如何生成/刷新噪声地图数据。

## Phase 3 · 运行时接入
- [ ] 引擎在缺少 `demo_world.json` 时回退加载 `generated/noise_mvp.json`，或提供配置开关。
- [ ] 确认资源系统沿用 `ratePerStep` 的刷新逻辑，处理生成图的新资源点。
- [ ] 设定代理初始落点：加载时挑选可通行节点并放置起始代理。
- [ ] 集成测试：通过运行时 API 验证地图加载成功且资源刷新无异常。

## Phase 4 · NPC 生命周期验证
- [ ] 准备脚本：在 `sandbox_cli` 中执行固定步骤序列（`step/pause/resume`）观察行为。
- [ ] 记录指标：确认 `MoveTo`→`ConsumeResource` 循环出现，`Hunger` 需求下降后回升。
- [ ] 添加调试/日志：必要时扩展 Telemetry 输出以便观察行为闭环。

## Phase 5 · CLI 可视化支持
- [ ] 基于生成的布局 JSON 渲染 ASCII 网格，显示 `A/M/C/F` 状态。
- [ ] 补充文档与截图/录屏指引，便于快速验证渲染效果。

### 验收回归清单
- [ ] 相同 seed 下生成文件一致，不同 seed 产生差异。
- [ ] 地形占比与阈值预期相符（阈值 0.5 ≈ 1:1）。
- [ ] `ResourceSpawn.current` 随时间趋近 `capacity`，消费时正确下降。
- [ ] NPC 在若干步内完成一次 `MoveTo` + `ConsumeResource` 循环。
- [ ] CLI 渲染对齐生成布局，可观察状态随时间变化。
