# GenesisEngine

GenesisEngine 是一个以“涌现式叙事模拟”为目标的 C++ 引擎核心，强调可扩展、可观测与可复用。核心模拟逻辑以 `genesis_runtime` 动态库对外提供统一 API，前端（GUI/Game）共享同一运行时。

## 快速开始
- 构建与运行沙盒 GUI：见 `docs/guides/sandbox_gui_smoke.md`
- Headless 回归与脚本回放：见 `docs/handbook/CORE_REGRESSION.md`、`docs/guides/headless_runtime_cli.md`
- 架构与设计原则：见 `docs/architecture/README.md`、`docs/architecture/VISION.md`
- 统一路线图与里程碑：见 `docs/roadmap/README.md`
- 进度总览与待办：见 `docs/status/progress-summary.md`、`docs/status/todo.md`

## 目录导航
- `docs/` 文档总览与导航见 `docs/README.md`
- `scripts/` 包含开发与演示脚本
- `build/` 为构建输出目录（建议使用 `cmake -S . -B build` 创建）

## 贡献
- 以 `docs/roadmap/README.md` 为单一规划来源；将临时待办从 `docs/status/todo.md` 迁移到 Issue/Milestone
- 优先补充单元/端到端测试；遵循可观测性约定（快照/遥测/回放）
