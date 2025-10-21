# Sandbox GUI 重构实施计划

依据《docs/architecture/sandbox_gui_ux_redesign.md》中的方案，将整体工作拆分为三大阶段（Phase A/B/C）。所有任务默认状态为 `pending`，后续执行过程中再更新状态并追加负责人、预估工时。

## Phase A · 布局与视觉统一
1. **UI 骨架重构**  
   - 建立新的主布局（Status Bar / Control Bar / Browser / Main View / Inspector）。  
   - 移除 Welcome 面板与命令队列面板；调整 DockBuilder 默认布局。  
   - 确认旧代码路径中任何直接引用被删除的面板均已替换或清理。  
   - ✅ 2025-10-21：完成顶/底栏重新布置、新 Browser & Main View 框架，并彻底移除 Welcome 与命令队列 UI；相关代码/测试已通过。
2. **视觉主题与设计令牌落地**  
   - 定义颜色、字号、间距、阴影等 design tokens。  
   - 更新全局 ImGui 主题，统一直角、分割线、间距。  
   - 梳理现有控件自定义样式，替换为新主题引用。  
   - ✅ 2025-10-21：新增 `DesignTokens` 模块集中管理颜色/间距，重写 AppHost 初始化应用统一主题；Status/Control/Main/Browser 视图按钮、提示与 Toast 配色全部改为令牌引用。
3. **Scene View 与附加层整合**  
   - 将 MapView 功能合并为 Scene 的节点关系叠加层。  
   - 实现叠加层切换按钮/快捷键（节点、网格、标尺等）。  
   - 确保 Browser 选中项与 Scene 高亮同步正常。
4. **Browser 框架重建**  
   - 支持分类（Scene/World/Monitor/Layouts & Themes）。  
   - 实现树状层级、搜索、过滤、记忆折叠状态。  
   - 接入主视图联动（切换分类时同步 Main View）。

## Phase B · 功能整合与体验优化
1. **Monitor 面板升级**  
   - 构建概览卡片、性能趋势图、资源摘要卡。  
   - 实现告警中心与任务概览入口。  
   - 初步落地统一时间轴（快照/命令/告警条目展示，含过滤与搜索）。
2. **日志与通知体系重写**  
   - 日志换行、级别配色、过滤器、批量复制/导出。  
   - Toast 按类型分区展示，写入时间轴并与日志联动。  
   - Control Bar 增加日志过滤快捷开关。
3. **Inspector 安全编辑流程**  
   - 字段元数据（只读/可写/危险）标注与 UI 适配。  
   - 磁铁/Alt 精调、Ctrl+点击恢复、右键精确输入。  
   - 危险操作自动快照、特殊 skin、撤销重做（Ctrl+Z/Y & Ctrl+Alt+Z/Y）。
4. **文件交互与世界生成改造**  
   - 引入原生文件对话框，记忆 last path。  
   - 支持拖拽加载世界/配置文件，Browser 条目附带操作按钮。  
   - 世界生成结果通过 toast + 时间轴反馈，移除命令队列 UI。

## Phase C · 高级能力与自定义
1. **时间轴深化与导出能力**  
   - 时间轴缩略图、区间导出（截图/录像）、批量选择。  
   - 事件类型扩展（自定义标签、用户标记、长任务进度）。  
   - 与 Control Bar 快照管理、回放功能联动。
2. **布局/主题与快捷键管理**  
   - Layout/Theme 列表预览、导入导出、恢复默认。  
   - Settings 中提供默认布局/主题/快捷键方案配置。  
   - 快捷键映射编辑器（冲突检测、搜索、导入导出）。
3. **批量编辑与权限/日志增强**  
   - Inspector 支持多选实体共用字段编辑与确认流程。  
   - 危险操作权限提示、日志记录来源。  
   - 为 QA/自动化测试提供必要的 hook。
4. **截图/录制与回放协同**  
   - 快捷键实时截图/录制的状态提示完善。  
   - 时间轴选择区间导出录像、单点生成截图并关联条目。  
   - 输出历史记录与路径管理整合进 Browser/Settings。

## 跨阶段架构重构任务（2025-10-21 新增）
1. **UI 模块解耦**  
   - 拆分 Status/Control/Browser/Main/Inspector 为独立视图类，`AppHost` 只负责装配。  
   - 建立共享 `UiContext`，明确状态归属与数据流向。  
   - ✅ 2025-10-21：完成 `UiState` 与 `UiContext` 抽离，为视图迁移提供统一入口。  
   - ✅ 2025-10-21：完成 StatusBar / ControlBar / Browser / MainView / Inspector 视图组件化并接入 AppHost。
2. **Presenter / ViewModel 架构**  
   - 为 Scene / World / Monitor 页签编写 Presenter，负责数据聚合与缓存。  
   - 为 Presenter 引入单元测试与回归样例，验证过滤、统计、排序逻辑。
   - ✅ 2025-10-21：完成 Scene/World/Monitor Presenter 拆分并补充回归测试，MainView 渲染逻辑与数据整形解耦。
3. **命令与事件总线**  
   - 实现 `WorldCommandController` 统一处理生成/加载/保存命令及状态轮询。  
   - 建立事件总线驱动 toast、日志、时间轴写入与快捷键反馈。
   - ✅ 2025-10-21：引入 `WorldCommandController` 承接命令状态刷新与提示，下沉 AppHost 直接操作 UI 状态的权限。
4. **布局/主题配置层**  
   - 设计可序列化的 `SandboxLayoutConfig` 与 `UiTheme`，供 Layout/Theme 管理与初始化使用。  
   - 将 DockBuilder、快捷键映射、主题颜色从代码常量迁移到配置层。
5. **源码拆分与测试支撑**  
   - 按页签拆分 `AppHostPanelViews.cpp` 并创建独立编译单元。  
   - 添加命令控制器、事件总线、布局序列化等核心模块的自动化测试。

## 交付与验证
- 每个阶段结束需要完成：代码实现、相关文档更新、手动/自动化测试计划。  
- 建议在 Phase A 完成后进行一次 UI 风格审查，Phase B 完成后做全面可用性回顾。  
- 所有阶段性成果应记录在时间轴与变更日志中，便于回溯与团队同步。
