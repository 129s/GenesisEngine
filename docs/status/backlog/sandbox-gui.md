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

3) MapView & Controls
- [ ] 加载布局并渲染节点/边/资源/代理；摄像机平移缩放。
- [ ] 控制面板（播放、暂停、步进、速度倍率、无限运行）。
- [ ] 名称标签与 Step 语义化（HUD：HH:MM:SS）。

4) Inspector & Telemetry（后续里程碑）
- [ ] 选择实体、显示详情与高亮（Inspector）。
- [ ] 曲线/表格：Hunger、库存、队列长度、帧耗、步进耗时。

5) WorldGen 面板
- [ ] 噪声参数表单（seed/width/height/threshold/density/capacity/rate）。
- [ ] 生成并热加载世界与布局；存/载生成配置。

6) 大图性能优化
- [ ] 视域裁剪、抽样绘制、批次提交；10k+ 节点流畅。

7) 打包与文档
- [ ] 二进制打包脚本（Win/Linux）；指南与截图、录屏建议。

## 验收（阶段性）
- MVP：1h 稳定运行无泄漏；10k+ 节点视图流畅（>30FPS）；名称标签与时间语义化可用；三名 OCEAN 画像的 Agent 可见差异选择；新增食物点生效。
- 后续：Inspector/Telemetry/WorldGen 可用；热生成功能不中断模拟（或短暂停顿但 UI 不冻结）。
