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
- 顶部菜单（File / View），View 菜单可切换 Demo、Map View、Telemetry
- Welcome 面板：帧率、背景色、VSync、播放控制（Pause/Resume/Step/倍速）、最新快照摘要
- Map View 面板：静态世界拓扑图（节点/边/资源）与 Agent 名称标签（可选插值/轨迹/图例）。
- Scene View 面板：基于整数网格的场景观察（当前为网格占位渲染），支持：
  - 节点下拉选择（Room/Point/Building/Region）；
  - 资源点（local_coord）与锚点（anchors）标注；
  - 相机平移（右键拖拽）与缩放（滚轮）。
  - 网格/锚点/资源显示开关。
- Telemetry 面板：当前帧的 Agents/Needs/Actions 摘要
- 底部状态栏：快速统计（Step/Agents/Resources/Actions）
- Log Console 面板：捕获 `spdlog` 输出并持续滚动（默认开启，支持手动关闭/自动滚动）。
- 可选 Dear ImGui Demo 窗口（验证 Docking 与基础组件）

> ⚠️ 远程或无图形环境运行时，窗口可能无法创建；请在本地含 GPU/桌面会话的环境中执行。

## Runtime 控制与快照
- Welcome 面板提供 Pause/Resume、Step、Step x10 按钮，以及 0.25x ~ 8x 速度调节。
- RuntimeBridge 在后台线程持续推进 `genesis::runtime::Runtime`，写入双缓冲 `SimulationSnapshot`（version/capturedAt/`TickTelemetry`，默认保留 96 帧）。
- GUI 线程每帧从快照缓冲读取最新数据，更新世界视图、Telemetry 与状态栏。

## Map View 面板
- 依据 `WorldRegistry` 中的 Location/Edge/Spawn 构建静态 2D 布局，按层级垂直排列。
- 节点颜色区分 Region / Building / Room / Point；资源生成点以三角标识。
- `Agents` 复选框可开启/关闭代理标记，`Trails` 控制是否绘制近期轨迹，支援 4~64 个采样点调节。
- 行为图例区分 Move/Consume/Idle/Other 四类行动，标记颜色与 Log/Telemetry 信息保持一致。
- 当前为静态画布：暂不支持平移/缩放、选中或实时布局更新。

## 已知限制
- Map View 暂无摄像机/缩放交互，布局使用 coord_global；位置插值依赖 movement_progress（可切换 Interpolate）。
  - 在 Map 上点击节点可打开 Scene View 并选中对应节点。

## Scene View 面板（占位版）
- 以整数网格为单位渲染，像素 tileSize 仅影响可视大小；
- 资源与锚点基于 `spawns.local_coord` 与 `edges.anchors`；
- 后续将接入 Tilemap 纹理与图层，实现完整主观察渲染。
- Telemetry 面板仅展示当前帧摘要，尚无历史曲线、筛选或导出功能。
- RuntimeBridge 默认节流至 ~500Hz（2ms 休眠），缺少自适应帧率/实时性能指标。
- Agent Overlay 仅基于离散 LocationId，不含精确坐标/碰撞；多代理重叠时标记会遮挡。
- 出于线程安全考虑，暂未在 GUI 侧直接读取 ECS 组件进行“移动插值”。如果需要更平滑的轨迹，应当在 Runtime/Telemetry 中产出插值所需的稳定字段（例如当前段进度），GUI 仅消费 Telemetry。
- 未封装命令行选项；运行期间也未暴露世界重新生成/热加载入口。
- Headless 环境仍不支持运行（依赖 OpenGL 上下文）。
- Windows 下默认隐藏控制台窗口，若需查看原始日志请使用 Log Console 或附加自定义 sink。

## 下一步
- 里程碑 3：完善控制台板（播放/步进/倍率 UI）、HUD 指标与性能采样。
- 里程碑 4：Inspector & Telemetry 曲线（实体选择、指标趋势）。
- 里程碑 5：WorldGen 参数面板与世界热加载。
- 请跟踪 `docs/roadmap/SANDBOX_GUI.md` 获取后续任务进度与待办。

## 故障排查
- 若在某些终端环境运行 GUI 出现卡死或约第 200 步崩溃，请参考 `docs/troubleshooting/sandbox_gui.md`。
