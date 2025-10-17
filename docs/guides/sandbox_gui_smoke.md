# Sandbox GUI · Smoke 快速上手

本指南涵盖里程碑 1（引导工程与空白壳）的构建与运行流程，帮助你验证 GLFW/OpenGL/ImGui 集成是否正常工作。

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
- 顶部菜单（File → Exit / View → Demo）
- Welcome 面板（帧率、背景色、VSync 切换、阶段说明）
- 可选 Dear ImGui Demo 窗口（验证 Docking 与基础组件）

> ⚠️ 远程或无图形环境运行时，窗口可能无法创建；请在本地含 GPU/桌面会话的环境中执行。

## 已知限制
- 仍为 Smoke 验证：未接入 `Genesis::Runtime`，没有世界渲染与数据面板。
- 单线程渲染循环；运行时 UI 帧率与模拟等价。
- 未封装配置/命令行选项；后续阶段将补充。

## 下一步
- 里程碑 2：引入 RuntimeBridge（后台推进 Runtime、快照缓冲）。
- 里程碑 3：构建控制面板（播放控制、HUD 指标）。
- 请跟踪 `docs/roadmap/SANDBOX_GUI.md` 获取后续任务进度与待办。
