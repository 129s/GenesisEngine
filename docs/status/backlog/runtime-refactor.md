# Backlog — Runtime Refactor

参考路线图：`../../roadmap/README.md`（近期 P0）

已完成（供历史参考）
- [x] 设计/实现双缓冲 `SimulationSnapshot`（`SimulationSnapshotBuffer` + Engine 回调），保证多线程安全。
- [x] 提炼 `GenesisRuntime` 公共 API（创建/销毁/推进/获取快照）。
- [x] 在 CMake 中导出 `genesis_runtime` 动态库，并调整现有目标依赖关系。
