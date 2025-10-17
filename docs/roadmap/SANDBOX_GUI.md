# Roadmap · Sandbox GUI（图形化沙盒）

## 背景与目标
- 背景：`sandbox_cli` 在大型地图下易出现频闪/撕裂，交互与观测能力有限，难以长期观察 NPC 全生命周期。
- 目标：提供一个轻量、跨平台的图形化沙盒，用以持续演进/调试引擎——可视化世界与 NPC 行为、实时调参（世界生成/需求/规划）、长时间运行稳定。

## 技术选型（建议）
- UI 框架：Dear ImGui（即时模式 UI）+ GLFW（窗口/Input）+ OpenGL 3.x 渲染后端。
  - 理由：
    - 部署轻、跨平台好；适合开发内工具（Docking、Inspector、Plot 等）。
    - 生态成熟，易集成到现有 CMake 流水线；对外部依赖控制较小。
  - 替代方案：Qt（完整桌面框架，成本/二进制体积更高）、SDL2（与 GLFW 二选一）。

## 架构概览
- AppHost：窗口/渲染循环/输入分发，ImGui 框架初始化（含 Docking）。
- RuntimeBridge：在后台线程运行 `Genesis::Runtime`；产出双缓冲/环形缓冲的 `TickTelemetry` 快照，供 UI 线程无锁读取。
- Panels：
  - WorldView：世界/布局渲染（节点/边/资源/代理、摄像机平移缩放、可见性裁剪）。
  - Inspector：选中代理/地点/资源，展示属性与关联信息。
  - Telemetry：折线/柱状图观察 Hunger/库存/队列长度等指标。
  - Controls：播放/暂停/步进/速度倍率、Step 限制（支持“无限”持续运行）。
  - WorldGen：噪声世界参数（seed/width/height/threshold/density/capacity/rate），一键重生并热加载布局。
  - Logs：滚动日志（spdlog sink）。
- DataFlow：Runtime 线程 -> 快照缓冲 -> UI 线程；UI 线程负责消费最新快照并请求（命令）修改运行态（例如重生世界/设置速度/步进）。

## 关键特性
- 无步数限制的持续运行；可按帧/按秒设置步进倍率，保证 UI 渲染与模拟解耦。
- 大图可视化优化：
  - 仅渲染视椎内实体（节点/边裁剪）；
  - 批次绘制/持久 VBO；
  - 可选“抽样显示/热度图”模式（减少重绘压力）。
- 世界生成热替换（WorldGen Panel）与布局热加载；生成参数持久化与共享（存 JSON）。
- 录屏/截屏友好：帧率限制、无闪烁、UI 隐藏/仅 HUD 模式。

## 验收标准
- 启动与退出稳定；在 1 小时持续运行中无崩溃、无明显内存泄漏。
- 10k+ 节点视图的平移缩放流畅（> 30 FPS），渲染裁剪生效；
- 通过 GUI 控件可：
  - 播放/暂停/步进/倍率调节；
  - 调整噪声参数并生成新世界，自动加载并继续运行；
  - 选择任意代理/资源并查看其当前状态/队列。
- 录屏场景：Legend/HUD 清晰、文本无闪烁，输出帧稳定。

## 里程碑
1) 引导工程与空白壳：GLFW+OpenGL+ImGui，渲染帧/输入回调/Docking（Smoke）。
2) RuntimeBridge：后台线程推进 Runtime，快照环形缓冲；WorldView 渲染节点/边（静态图）。
3) 控制台板：Play/Pause/Step/Speed；`--auto-run` 与 CLI 等价；HUD 显示基本指标。
4) Inspector & Telemetry：选择实体、绘制指标曲线；性能采样（帧耗、步进耗时）。
5) WorldGen 面板：噪声参数 UI、生成与热加载布局，保存/导入世界。
6) 大图优化：裁剪/批次/抽样；支持 10k+ 节点流畅渲染。
7) 打包与文档：二进制打包脚本（Win/Linux），用户指南与截图。

### 当前阶段（里程碑 1 · 引导工程与空白壳）
- [x] 建立 `genesis_sandbox_gui` 可执行程序目标，集成 GLFW/OpenGL 上下文创建。
- [x] 引入 Dear ImGui，并启用 Docking/Viewport 支持，完成基础 UI 帧循环。
- [x] 提供最简 Render 管线（清屏 + ImGui 渲染），验证输入回调与窗口 Resize。
- [x] 补充开发者指南：构建/运行 GUI 沙盒的步骤与已知限制（Smoke 级别）。

**阶段产出（Smoke）**
- 新增 `genesis-sandbox-gui`：GLFW 3.3 + OpenGL Core + Dear ImGui Docking，提供 DockSpace + Welcome 面板 + Demo Window 切换。
- GUI 与渲染线程同线程运行，验证窗口生命周期、SwapInterval/VSync 控制、主菜单/欢迎面板基础交互。
- `docs/guides/sandbox_gui_smoke.md` 记录构建/运行/已知问题；下一阶段聚焦 RuntimeBridge 与世界渲染接入。

### 当前阶段（里程碑 2 · RuntimeBridge + 静态世界视图）
- [x] `RuntimeBridge`：后台线程推进 `Genesis::Runtime`，支持暂停/单步/倍率控制接口，使用线程安全快照缓冲。
- [x] `WorldAtlas`：提取世界拓扑（Location/Edge/Spawn），生成静态 2D 布局数据供渲染使用。
- [x] 世界视图面板：利用 ImDrawList 绘制节点/边/资源占位符；显示模拟步数、代理/资源统计。
- [x] Telemetry 面板雏形：展示最近一帧的关键指标，验证快照读取正确性。
- [x] 指南更新：补充运行线程模型、已知限制（单线程渲染 + 后台模拟）、调试建议。

**阶段产出（RuntimeBridge）**
- `RuntimeBridge` 后台线程 + 快照环形缓冲，提供 Pause/Resume/Step/Speed 控制接口。
- 静态 `WorldAtlas` 布局：按层级生成节点坐标、资源标记，世界视图面板即时绘制。
- 新增 Telemetry/状态栏面板，展示当前快照数据；Welcome 面板整合播放控制与摘要。
- 集成 Log Console 面板捕获 `spdlog` 日志，并在 Windows 环境下隐藏控制台窗口。
- `docs/guides/sandbox_gui_smoke.md` 扩展至里程碑 1&2 的构建、运行与限制说明。

### 下一阶段（里程碑 3 · 控制台板 & HUD 指标）进行中
- [ ] 将 Playback 控件拆分为专用 Control Panel（快捷键、自动运行预设）。
- [ ] HUD 指标：运行时帧耗、模拟耗时、快照延迟等性能可视化。
- [ ] 快照统计：展示历史步长/代理/需求趋势的迷你图或 sparkline。
- [ ] RuntimeBridge：暴露性能计数器（处理耗时、快照生成耗时）。
- [ ] 文档：新增调试/性能诊断章节，说明控制面板快捷键与指标含义。

## 风险与规避
- 渲染跨平台兼容：以 OpenGL 3.x 为基线；提供禁用 MSAA/降级路径。
- 线程安全：仅通过命令与快照交互；避免 UI 直接操作引擎内部结构。
- 依赖治理：通过 CPM/FetchContent 管理 glfw/imgui，防止版本漂移；CI 增加构建验证。
