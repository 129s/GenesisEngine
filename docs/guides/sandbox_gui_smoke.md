# Sandbox GUI · Smoke & RuntimeBridge 快速上手

本指南涵盖：
- 里程碑 1（Smoke）：验证 GLFW/OpenGL/ImGui 基础壳体。
- 里程碑 2（RuntimeBridge）：后台推进 Runtime、静态世界视图与首批面板。

## 环境准备
- CMake ≥ 3.21
- 支持 C++20 的编译器（MSVC / Clang / GCC）
- Windows：建议安装最新显卡驱动；如果远程/无显示环境，运行时可能无法创建 OpenGL 上下文。

依赖管理通过 `CPM.cmake` 自动完成：GLFW 3.3、Dear ImGui 1.91.0 docking 分支。

## 构建步骤
```bash
cmake -S . -B build
cmake --build build --target genesis_sandbox_gui
```

首次构建会自动下载并编译第三方依赖，时间较长；后续增量编译仅受增量源码影响。

## 运行方式
可执行文件位于 `build/src/genesis-sandbox-gui.exe`（平台后缀视系统而定）：
```bash
./build/src/genesis-sandbox-gui.exe
```

启动后可看到：
- Docking 主视口（可自由拆分窗口）
- 顶部菜单（File / View），View 菜单可切换 Demo、World View、Telemetry
- Welcome 面板：帧率、背景色、VSync、播放控制（Pause/Resume/Step/倍速）、最新快照摘要
- World View 面板：静态世界拓扑图（节点/边/资源）
- Telemetry 面板：当前帧的 Agents/Needs/Actions 摘要
- 底部状态栏：快速统计（Step/Agents/Resources/Actions）
- 可选 Dear ImGui Demo 窗口（验证 Docking 与基础组件）

> ⚠️ 远程或无图形环境运行时，窗口可能无法创建；请在本地含 GPU/桌面会话的环境中执行。

## Runtime 控制与快照
- Welcome 面板提供 Pause/Resume、Step、Step x10 按钮，以及 0.25x ~ 8x 速度调节。
- RuntimeBridge 在后台线程持续推进 `Genesis::Runtime`，复制最新 `TickTelemetry` 快照（默认保留 96 帧）。
- GUI 线程每帧从快照缓冲读取最新数据，更新世界视图、Telemetry 与状态栏。

## World View 面板
- 依据 `WorldRegistry` 中的 Location/Edge/Spawn 构建静态 2D 布局，按层级垂直排列。
- 节点颜色区分 Region / Building / Room / Point；资源生成点以三角标识。
- 当前为静态画布：暂不支持平移/缩放、选中或实时布局更新。

## 已知限制
- World View 暂无摄像机/缩放/选中交互，布局为简单层级排布。
- Telemetry 面板仅展示当前帧摘要，尚无历史曲线、筛选或导出功能。
- RuntimeBridge 默认节流至 ~500Hz（2ms 休眠），缺少自适应帧率/实时性能指标。
- 未封装命令行选项；运行期间也未暴露世界重新生成/热加载入口。
- Headless 环境仍不支持运行（依赖 OpenGL 上下文）。

## 下一步
- 里程碑 3：完善控制台板（播放/步进/倍率 UI）、HUD 指标与性能采样。
- 里程碑 4：Inspector & Telemetry 曲线（实体选择、指标趋势）。
- 里程碑 5：WorldGen 参数面板与世界热加载。
- 请跟踪 `docs/roadmap/SANDBOX_GUI.md` 获取后续任务进度与待办。
