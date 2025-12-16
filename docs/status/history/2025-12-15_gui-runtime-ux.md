# 归档：Sandbox GUI / Runtime / UX 集中迭代记录（2025-12-15）

> 说明：该文档为阶段性“实现 + 体验改进”记录，方便回顾 GUI 方向曾经做过什么。当前项目主线已切换为 **Core 模拟 + 回归/Soak/Worldline 工具链**，GUI 相关工作暂缓推进。

## 当次进展摘要（归档）
- Sandbox GUI 窗口体验：
  - 启动时自动根据主显示器工作区居中放置窗口，便于多显示器快速定位。
- Sandbox GUI 主题：
  - 基于 `build/src/serum.png` 精确取样重写 `DesignTokens` 背景/主色/语义色，统一交互亮度与描边对比。
  - 新增 `docs/rendering/ui-palette.md` 说明色板与状态矩阵，为后续控件迁移提供基线。
- Sandbox GUI 浏览器：
  - 调整列表项 FramePadding，统一抬高行高，提升条目可读性和指针命中空间。
  - 统一目录条目点击/聚焦配色，选中后 Hover 保持同色以消除交互瞬时闪烁。
  - 补齐面板内边距、统一树节点缩进，并将详情卡片改为浮层覆盖，缩短上下留白后默认展示区域增大且仅目录树滚动。
  - 分割线统一为 2px，并在拖拽时使用 Accent 高亮，同时扩大命中区域；全局启用同宽的 ImGui docking 分隔线保持面板一致性。
- Docking 操作体验：
  - 提升 ImGui docking 分割线厚度至 6px，并增加 TouchExtraPadding，扩大鼠标命中范围，布局调整更顺手。
- Sandbox GUI 结构重构：
  - 将 `AppHost` 单体实现拆分成 `AppHostCore/AppHostLayout/AppHostPanelWorld/AppHostPanelViews` 多个编译单元，并抽出 `CommandUiHelpers`、`FilesystemHelpers`、`ImGuiLogSink` 等私有头以复用逻辑。
  - CMake 目标 `genesis_sandbox_gui` 已更新引用新的模块化结构，后续可按面板粒度维护与扩展。
  - 控制栏与状态栏改为可停靠窗口，并提供缺省 Dock 布局，避免覆盖主视图区。
- 运行时快照 diff 与事件注入 API：
  - 新增 `Runtime::latestSnapshotDiff()` 与 `SimulationSnapshotDiff`，支持快速检测资源/需求/行动变化并携带事件日志。
  - 引入 `Runtime::enqueueEvent` 命令队列，在每个模拟步执行并写入 `RuntimeEventReport`，用于 GUI 交互与自动化回放。
  - 扩展双缓冲快照结构，捕获命令执行结果并在文档中更新使用指南。
- 命令队列与 GUI 控制台迭代：
  - `Runtime::enqueueEvent` 现返回事件 ID，支持 `runtimeHandler`/`onComplete`，并在单元测试中验证消息链路。
  - `RuntimeBridge` 维护命令 pending/history 状态，新增 JSON 命令序列与世界加载/保存 handler（示例见 `data/scripts/world_cycle.json`，推荐 `world.db.*`）。
  - World IO 面板通过命令队列提交 `world.db.load/save/reload`，避免 UI 线程直接操作运行时对象，降低线程竞争风险。
- Sandbox GUI Inspector 基础版：
  - Inspector 面板新增实体列表（Agent/Resource/Node 分组），详情面板支持查看需求、行动、Planner 结果与移动进度；搜索能力现统一迁移到 Browser。
  - Map View 增加选中高亮、Agent 圆环强调及“一键定位/Scene 打开”按钮；支持跟随模式自动切换 Scene View。
  - 将 Runtime 事件日志与需求 diff 整合进 Inspector，便于定位命令执行结果与本帧变化。
- 构建与测试稳定性改进：
  - 移除遗留的 `genesis_sandbox_cli` 目标及其测试脚本，避免重复维护 ASCII 工具链。
  - 为 `genesis_runtime_tests` 增加 `Genesis::Engine` 链接，避免 Windows 下跨模块静态库初始化差异带来的崩溃。
  - 在 `Runtime` 中初始化 SPDLOG 缺省 logger（最佳努力，不干扰外部设置）。
  - 全部测试通过（33/33）。
- 世界加载与可达性：
  - 放宽世界载入对部分字段的强制要求，兼容部分历史 JSON；存在时解析回填（当前主线契约以 v2 WorldDatabase 为准）。
  - 修正初始化出生点策略：优先选择“可达的资源所在位置”，避免噪声图孤岛导致长期无法消费。

