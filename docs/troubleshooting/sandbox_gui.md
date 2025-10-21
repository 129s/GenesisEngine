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

## 2025-10-22 · 暂停后 CPU 仍持续占用

症状：
- 在 Sandbox GUI 内将模拟切换到暂停状态后，后台进程 CPU 仍维持在约 2% 左右。
- 任务管理器/活动监视器显示 `genesis_sandbox_gui` 的线程无视 pause 命令仍在运行。

根因：
- `AppHost::updateRuntimeSnapshot()` 每帧都会调用 `RuntimeBridge::latestSnapshot()`，而该函数按值返回 `Snapshot`。`Snapshot` 本身包含 `TickTelemetry`（资源、需求、行动队列、代理、移动等多个大型数组）、事件日志、命令状态与预计算坐标。
- 即使暂停后 `RuntimeBridge` 不再推进模拟，新帧循环仍会逐帧复制整份结构，造成大量内存拷贝和 CPU 消耗。参见：
  - `include/sandbox/gui/RuntimeBridge.hpp` 的 `Snapshot` 定义；
  - `src/sandbox/gui/RuntimeBridge.cpp:222` 对 `snapshots_.back()` 的复制；
  - `src/sandbox/gui/AppHostCore.cpp:312-340` 的快照轮询逻辑。

排查与修复建议：
1. **返回轻量引用**：让 `latestSnapshot()` 返回 `shared_ptr<const Snapshot>` 或内部持有的 `const Snapshot&`，GUI 只读访问，不触发深拷贝。
2. **版本节流**：在 `AppHost::updateRuntimeSnapshot()` 缓存 `Snapshot::version` 或 `capturedAt`，若无更新则跳过拷贝和 `updateAgentTrails()` 等后续处理。
3. **暂停/失焦降频**：暂停或窗口失焦时考虑改用 `glfwWaitEventsTimeout()`，降低 UI 刷新频率，减少 `renderGui()` 的空转。
4. **差分惰性计算**：`RuntimeBridge::captureSnapshot()` 中的 `runtime_.latestSnapshotDiff()`可改为按需触发，避免暂停期间持续做 diff。

验证思路：
- 打开 Sandbox GUI，进入 Monitor 面板，暂停后通过性能分析工具确认 CPU 下降。
- 重点观察 `RuntimeBridge` 内部线程与主线程的 `latestSnapshot()` 调用频率是否显著降低。

## 2025-10-17 · 新数据契约下渲染缺失/错位

症状：
- Map View 不显示部分节点或位置错位；Scene View 无法渲染代理入/出场或资源点不见。

根因（世界数据未满足新契约）：
- `locations[i].coord_global` 缺失 → Map 无法定位节点。
- `edges[i].anchors` 缺失或不合法 → Scene 入/出场锚点无参照。
- `spawns[i].local_coord` 缺失 → 资源/交互点无局部坐标。
- `schema_version` 不匹配 → 前端按旧协议解析导致字段缺省。

排查与修复：
1) 检查世界 JSON 是否包含上述字段（参见 `docs/architecture/WORLD_MODEL.md` 与迁移指南）。
2) 确认 `schema_version` 与 Runtime/GUI 约定一致。
3) 使用最小示例数据验证渲染（来自迁移指南的片段）。
4) Loader 报错：自 2025-10 起，WorldLoader 将强校验上述必备字段，缺失会返回错误并拒绝加载（不再提供旧版回退路径）。
