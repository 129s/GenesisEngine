# Backlog · Sandbox GUI

## Epic
- 以 GUI 形态替代 CLI 作为主要开发内工具，解决大图频闪与交互不足问题，支持无步数限制的持续运行与实时调参。

## Milestones
1) 引导工程
- [ ] 新增目标 `genesis_sandbox_gui`（GLFW + OpenGL3 + Dear ImGui），渲染 Hello/Docking（Smoke）。
- [ ] CMake 引入依赖（CPM/FetchContent），CI 编译检查。

2) RuntimeBridge
- [ ] 后台线程推进 Runtime（Play/Pause/Step/Speed）。
- [ ] 快照环形缓冲（UI 读、Runtime 写）与命令通道（UI→Runtime）。

3) WorldView & Controls
- [ ] 加载布局并渲染节点/边/资源/代理；摄像机平移缩放。
- [ ] 控制面板（播放、暂停、步进、速度倍率、无限运行）。
- [ ] Legend/HUD 与帧率/步进速率显示。

4) Inspector & Telemetry
- [ ] 选择实体、显示详情与高亮路径。
- [ ] 曲线/表格：Hunger、库存、队列长度、帧耗、步进耗时。

5) WorldGen 面板
- [ ] 噪声参数表单（seed/width/height/threshold/density/capacity/rate）。
- [ ] 生成并热加载世界与布局；存/载生成配置。

6) 大图性能优化
- [ ] 视域裁剪、抽样绘制、批次提交；10k+ 节点流畅。

7) 打包与文档
- [ ] 二进制打包脚本（Win/Linux）；指南与截图、录屏建议。

## 验收
- 1h 稳定运行无泄漏；
- 10k+ 节点视图流畅（>30FPS），Inspector/Telemetry/WorldGen 功能可用；
- 热生成功能可用且不中断模拟（或有短暂停顿但 UI 不冻结）。

