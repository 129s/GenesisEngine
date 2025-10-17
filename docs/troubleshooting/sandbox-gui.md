# Sandbox GUI 故障排查

本文汇总近期 `genesis_sandbox_gui` 相关已知问题与修复建议。

## 2025-10-17 · 启动后部分环境未响应 / 约第 200 步闪退

症状：
- 在部分终端（交互式 CLI）直接运行 GUI 时仅显示 Welcome 面板且程序“未响应”。
- 在资源充足的本地环境中双击或在其它终端运行时，约第 120~201 步附近崩溃（SIGSEGV / 断言失败）。

根因：
- 引入“代理位置插值”后，`RuntimeBridge::resolveAgentPosition` 在 GUI 后台线程读取了 ECS (`entt::registry`) 中的 `MovementState`，该访问与模拟线程并发，存在数据竞争与生命周期问题，触发 `std::vector<MovementState*>` 越界断言或段错误。

修复：
- 移除 GUI 线程对 `registry` 的直接读取，改为仅依据 `TickTelemetry` 的离散 `LocationId` 通过 `WorldAtlas` 映射静态坐标渲染。
- 变更点：
  - 删除 `RuntimeBridge::resolveAgentPosition`，在 `captureSnapshot` 中不再查询 ECS，改为 `atlas.nodePosition(agent.location)`。
  - 后续若需平滑插值，应在 Runtime 侧产出稳定的插值所需数据（如 segment 进度）并写入 `Telemetry`，由 GUI 只读消费。

验证：
- 本地 Debug/Release 均可持续运行多轮（> 3600 步）无崩溃，CLI 环境运行亦不再卡死。

注意：
- Windows 远程/无图形会话可能无法创建 OpenGL 上下文，仍建议在有桌面/驱动的环境运行，详见 `docs/guides/sandbox_gui_smoke.md`。

