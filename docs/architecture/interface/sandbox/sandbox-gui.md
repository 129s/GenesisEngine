# Architecture · Sandbox GUI

Sandbox GUI 是主要的可视化与调试前端，消费 Runtime 的 Telemetry/WorldAtlas 数据完成世界渲染、指标监控与交互控制。本文概述模块划分、线程模型、性能策略与路线图。

## 模块划分
- **AppHost**
  - 管理 GLFW 窗口、渲染循环、输入系统、ImGui 初始化（Docking/多视口可选）。
  - 负责加载主题、保存/恢复窗口布局、驱动帧计时与调试面板注册。
- **RuntimeBridge**
  - 后台线程推进 `genesis::runtime::Runtime`；负责命令队列、Telemetry 双缓冲、Atlas 版本管理。
  - 提供 `withLatestTelemetry(fn)`、`submitCommand(cmd)` 等接口，确保线程安全。
  - 当 `world_version` 更新时刷新 Tilemap 缓存、触发面板通知。
- **Panels（ImGui）**
  - `MapView`：渲染 Scene/Interactive 节点（含资源、Portal 等），基于 `coord_global` 绘制拓扑，并提供摄像机控制、图例、过滤。
  - `SceneView`：渲染当前 map 的 Tilemap（inside/outside），支持平移/缩放/图层开关/网格；与 MapView 选中同步。
  - `Inspector`：展示实体属性、需求、人格、行动队列；支持 Follow、Teleporter。
  - `TelemetryPanel`：绘制指标曲线（属性/需求强度、资源库存、系统耗时）。
  - `Controls`：播放控制、速度倍率、单步、截图/录制；显示 `world_version`、`schema_version`。
  - `WorldGen`：热更新 MapConfig，触发世界重生成，并展示校验结果。

## MapView 交互（2025-10-20）
- 滚轮缩放与右键拖拽平移已上线，面板顶部提供 `Reset View` 按钮及 `Space` 快捷键复位相机。
- 缩放倍率低于阈值时自动隐藏节点标签与资源条，仅在悬停或选中时强制显示，缓解密集场景遮挡。
- 面板顶部同步展示交互提示（`Scroll zoom | Right-drag pan | Space reset`），降低探索成本。

## 线程模型
- **Runtime 线程**
  - 固定周期推进模拟（或按 UI 设定的 speed 调整）。
  - 将最新 `TickTelemetry` 写入环形缓冲；监听命令通道（无锁队列/原子标志）。
  - 世界重建后更新 `WorldAtlas` 引用并提升 `world_version`。
- **UI 线程**
  - 每帧从 RuntimeBridge 获取最新 Telemetry/Atlas（若 `world_version` 变化则重建渲染缓存）。
  - 渲染面板、处理输入、发出命令（`play/pause/regenWorld` 等）。
  - Tilemap 纹理、Portal 可视化缓存由 UI 线程管理，避免跨线程 OpenGL 调用。

## 性能策略
- **可见性裁剪**：MapView/SceneView 只绘制视野内对象；对大世界支持分块加载。
- **批次绘制**：节点/边用批量提交或 instancing；Tilemap 支持裁剪矩形减少 draw call。
- **缓存管理**：Tilemap 纹理缓存 + LRU，Atlas 版本变更时刷新；Portal/锚点 geometry 预烘焙。
- **解耦帧率**：UI 帧率与模拟步进分离（UI 可低帧但保证输入响应），提供 FPS/Step HUD 监控。

## 集成点
- 依赖：spdlog、nlohmann_json、EnTT（共享核心）；新增 GLFW、Glad/OpenGL3、Dear ImGui、ImPlot（可选）。
- CMake：`add_executable(genesis_sandbox_gui ...)`，使用 CPM/FetchContent 获取依赖；资源拷贝（Tilemap、字体、Shaders）在 post-build 完成。
- 与 Runtime API：严格通过 `IRuntimeControl`/`IRuntimeQuery` 交互，不直接访问 ECS。

## 路线图参考
1. **MVP**：RuntimeBridge、MapView、Controls；基础 Telemetry 可视化，支持世界刷新。
2. **P1**：Inspector、Telemetry 曲线、SceneView 基础渲染、Portal 高亮。
3. **P2**：Tilemap 优化（裁剪/缓存/二进制格式）、多窗口布局保存、截图/录像。
4. **P3**：Chunk 级流式加载、性能分析工具、UI 自动化测试挂钩。

更多细节见：
- [`sandbox-gui-sim-loop.md`](./sandbox-gui-sim-loop.md)：模拟循环、人格 Demo、验收标准。
- [`sandbox-gui-tilemap-rendering.md`](./sandbox-gui-tilemap-rendering.md)：Tilemap 资源、渲染管线、二进制协议。
- [`inspector-panel.md`](./inspector-panel.md)：Inspector 交互设计与数据要求。
