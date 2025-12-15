# TODO Backlog

> 提示：本清单为临时待办收集处，阶段性规划请以 `../roadmap/README.md` 为准。有效条目建议迁移到仓库 Issue 与 Milestone 跟踪。

## P0 · Sprint-2（建议聚焦）
- [x] Runtime 命令队列接入 GUI `RuntimeBridge`，提供事件注入/回放示例脚本与最小 UI 触发。（World Generation 面板改为命令队列，新增脚本加载与状态表）
- [ ] 建立 Sandbox GUI 自动化回归：端到端闭环测试（Planner → Executor → Need 恢复）与 24 小时 soak 指标脚本。
- [ ] Telemetry/快照协议文档更新：统一 schema 版本，扩写 `docs/architecture/foundation/runtime-api.md`、Inspector 字段说明，并在 GUI 中校验版本。

## Runtime refactor
- [x] 提炼 `GenesisRuntime` 公共 API（创建/销毁/推进/获取快照）。
- [x] 在 CMake 中导出 `genesis_runtime` 动态库，并调整现有目标依赖关系。
- [x] 设计/实现双缓冲 `SimulationSnapshot`，保证多线程安全（同时提供快照 diff 与命令队列）。
- [x] Runtime 命令队列接入 GUI `RuntimeBridge`，提供回放/标记示例脚本（↑ Sprint-2 核心）。

## Noise Map MVP
- [x] Phase 5 完成：参照 `docs/status/backlog/noise-map-mvp.md`（可视化指引与 GUI 增强已落地）。

## Scene/Interactive 重构（MVP）
- [ ] Scene 子节点布局描述解析，填充 `coord_local`/`coord_global`（兼容旧数据）。
- [ ] Portal 锚点 `anchors{ at_from, at_to }` 读写契约与运行时查询。
- [ ] GUI 渲染读取锚点/交互坐标并绘制调试标记。

## Sandbox GUI（新）
- [ ] GUI 框架引导（GLFW + OpenGL + ImGui），新目标 `genesis_sandbox_gui`。
- [ ] RuntimeBridge：后台线程推进 + 快照环缓冲；基础控制面板。
- [ ] MapView：布局渲染/摄像机/Legend；Inspector/Telemetry。
- [ ] WorldGen 面板与热生成；大图性能优化。
- [x] Inspector 面板基础版（实体列表/详情/地图联动，支持事件日志）。
- [ ] Inspector 深化：人格/Traits、Portal 详情、历史趋势等（详见 `docs/architecture/INSPECTOR_PANEL.md` 待完善清单）。
- [ ] Telemetry 面板指标卡片与阈值配置。

## Tooling & Docs
- [x] 更新测试，验证共享库加载及 GUI 运行。
- [x] 扩写 `architecture.md` 与运行时使用说明。
- [ ] 预留后续 `sandbox_gui`/`game` 接入指引。

### 已拆分的 Backlog（便于迁移为 Issue）
- 运行时重构：`docs/status/backlog/runtime-refactor.md`
- Sandbox GUI：`docs/status/backlog/sandbox-gui.md`
- Tooling & Docs：`docs/status/backlog/tooling-docs.md`


## Sandbox GUI · UX 改进（依据 2025-10-19 评审）
- [x] Toolbar：将播放/步进/倍速与 VSync 抽取为全局控制条（Welcome 仅保留状态）。
- [x] World Generation：默认折叠命令队列，提供 Pending/Success/Failed 过滤与“Clear Completed”；执行结果使用 toast 提示。
- [x] GUI 文案统一为英文；[ ] 补充 Tooltip 与剩余表头优化。
- [x] MapView：节点内图标+悬浮tooltip、标签重叠抑制（已落地）；[x] 节点选择+父/子连线按需显示；[x] 文本缩放阈值隐藏；[ ] 选中高亮一致化；[x] 交互提示与“复位视图”。
- [x] Inspector：Follow/Focus/Open Scene 置于详情标题行；[x] 支持复制所选实体快照 JSON。
- [ ] Log Console：底部 Dock、级别过滤/搜索/清空；[x] F8 快捷键开关。
- [ ] 配色规范：资源/行动/选中/警告的颜色体系文档化并落地到代码常量。




