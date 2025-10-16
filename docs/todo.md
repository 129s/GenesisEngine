# TODO Backlog

## Runtime refactor
- [ ] 提炼 `GenesisRuntime` 公共 API（创建/销毁/推进/获取快照）。
- [ ] 在 CMake 中导出 `genesis_runtime` 动态库，并调整现有目标依赖关系。
- [ ] 设计/实现双缓冲 `SimulationSnapshot`，保证多线程安全。

## Sandbox CLI
- [ ] 重构现有 ASCII 播放器为 `sandbox_cli`，实时调用运行时 API。
- [ ] 支持基本控制命令：`pause`、`resume`、`step`, `inspect agent <id>`。
- [ ] 保留 ASCII 视图，基于实时快照刷新，而非读取离线 JSON。

## Tooling & Docs
- [ ] 更新测试，验证共享库加载及 CLI 运行。
- [ ] 扩写 `architecture.md`/`ascii-viewer.md` 中的运行时使用说明。
- [ ] 预留后续 `sandbox_ui`/`game` 接入指引。
