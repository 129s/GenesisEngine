# Architecture · Sandbox GUI

## 模块划分
- AppHost
  - 责任：GLFW 窗口与渲染循环、输入系统、ImGui 初始化（Docking/多视口可选）。
- RuntimeBridge
  - 责任：在后台线程推进 `Genesis::Runtime`；提供线程安全的 `TickTelemetry` 读取接口（双缓冲/固定大小环形缓冲）。
  - 命令通道：UI 调用 `play/pause/step(n)/setSpeed(x)/regenWorld(params)` 等指令，桥接到 Runtime 线程执行。
- Panels（ImGui）
  - WorldView：渲染世界图（节点/边/资源/代理），摄像机控制（平移、缩放、重置），显示 Legend/HUD；仅渲染视野内对象。
  - Inspector：显示选中实体（Agent/Location/Spawn）的详情，支持高亮与跟随。
  - Telemetry：曲线/表格（Hunger/库存/队列长度/帧耗）。
  - Controls：播放控制与速度倍率；步数无限运行切换；截图/导出。
  - WorldGen：噪声参数表单与一键生成；生成配置保存/导入。

## 线程模型
- Runtime（后台线程）：
  - 固定周期推进（或按 UI 设置的 speed/步进倍率推进）；
  - 将最新 `TickTelemetry` 写入 Producer 缓冲；接收 UI 发送的命令（原子标志/无锁队列）。
- UI（主线程）：
  - 每帧读取 Consumer 缓冲中的最新快照（尽量无锁/无拷贝）；
  - 消费快照进行渲染与面板更新；
  - 通过命令通道发起运行控制与世界再生成。

## 性能策略
- 可见性裁剪：基于摄像机视域裁剪；大图仅绘制屏内节点/边；可选抽样或小点模式。
- 批次绘制：节点/边使用批量提交，减少 Draw Call；静态几何放入持久 VBO。
- UI 与模拟解耦：UI 帧率与模拟步进解耦；允许 UI 低帧而模拟高步，或反之。

## 集成点
- 依赖沿用：spdlog、nlohmann_json、EnTT；新增：GLFW、Dear ImGui（及 OpenGL3 后端）。
- CMake：`add_executable(genesis_sandbox_gui ...)`；使用 CPM/FetchContent 获取第三方依赖；分平台编译选项与拷贝资源。

## 路线图映射
- MVP：WorldView + Controls + RuntimeBridge（Play/Pause/Step/Speed）
- P1：Inspector + Telemetry；WorldGen 面板热生成
- P2：大图优化与绘制性能；录屏/截图优化；多窗口布局保存

