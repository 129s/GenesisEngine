# Sandbox GUI · 界面问题分析与优化方案（2025-10-19）

> 范围：基于当前 Sandbox GUI 截图与代码（src/sandbox/gui/AppHost.cpp 等）进行一次轻量 UX 评审，聚焦信息架构、可读性与操作流畅度；结论用于指导下一个迭代周期（1–2 周）。

## 主要问题
- 信息架构分散：控制（暂停/步进/倍速）、世界生成、日志、遥测、Inspector 分布在多个窗口（Welcome/World Generation/Log/Telemetry/Inspector），需要频繁在小窗之间切换；中部还夹着“World Generation”导致主视图区被挤占。
- 语言不一致：标题多为英文（Welcome/Inspector/Log Console），内容与按钮多为中文，术语中英混用；部分中文标签被截断（列宽不够）。
- 视觉密度高：窗口边框、分隔线、表格线较多，留白不足；节点标签与连线/图标重叠，图例与复选项占据上方空间，影响地图可读性。
- 交互发现性弱：
  - Map View 的缩放/拖拽手势缺少提示；节点点击可打开 Scene View 的能力缺少引导。
  - Inspector 列表与详情上下分布，但 “定位到地图/打开 Scene/跟随” 等核心动作分散在详情末端，不容易被发现。
  - World Generation 的“命令队列状态”较长且默认展开，刷屏影响其他操作。
- 面板布局耦合：日志、生成、Inspector 同时常驻会占据大量空间；Log 与 Inspector 的优先级不同，但层级同等。
- 颜色语义未固化：节点/资源/行动的颜色方案缺少一致性说明；状态强调（高亮/选中/告警）与常规颜色差异较小。
- 可达性与状态反馈：执行“生成/加载/保存/脚本”后仅在面板中显示文字，缺少弹出式反馈或统一的 toast/通知；失败与成功样式近似。
- 状态栏信息密：底部状态栏展示 Step/Agents/Resources/Actions，但字号较小、与其他信息重复（Telemetry/Welcome 也显示）。

## 优化目标
- 聚焦“观察-调试-迭代”的闭环：以 Map/Scene + Inspector 为主舞台，其他面板（Log/WorldGen/Telemetry）降级为工具抽屉。
- 降低操作成本：集中控制入口（播放/步进/倍速/跟随），减少视线与鼠标移动。
- 提升可读性：统一语言与字号、简化边框/线条，保证标签不截断并提供 Tooltip。
- 强化反馈：统一的任务/命令通知系统，成功/失败一目了然，可跳转定位。

## 重构方案（分阶段）

### Phase 0 · 快速改进（1–2 天）
- 顶部“控制条（Toolbar）”：从 Welcome 抽取暂停/步进/倍速与 VSync，置于单行工具条（drawMainMenuBar 下方或与之合并）。
- 压缩/折叠 World Generation：默认折叠“命令队列状态”，新增“仅显示失败/进行中”过滤与“清除已完成”。
- 统一命名与本地化：窗口/菜单统一中文（或提供 i18n 开关），按钮与表头文案统一（例如 “状态/来源/备注”）。
- 地图上标签避让：为节点文本增加少量外边距；当缩放过小时自动隐藏文本，仅保留圆点与高亮。
- Inspector 详情顶部放置关键动作：Follow/定位到地图/打开 Scene 置于名称行右侧，保证低滚动可见。

### Phase 1 · 版面与导航（1 周）
- Dock 预设布局：
  - 中心：Map View（默认）与 Scene View（Tab 切换）。
  - 右侧：Inspector（上：列表，下：详情）。
  - 底部：Log Console（可隐藏，F8 快捷键切换）。
  - 侧边抽屉：World Generation 与 Telemetry（通过侧边按钮打开）。
- 统一状态反馈：
  - 引入轻量 	oast/notification（右上角 3 秒消失，点击可跳转相关面板）。
  - 命令执行进度以气泡提示最新条目，失败使用红色并附“复制错误”。
- Map 交互提示：悬停显示“左键选中/右键拖拽/滚轮缩放”小贴士；提供“复位视图”按钮。

### Phase 2 · 语义与图形（1–2 周并行）
- 颜色体系：定义状态色板（资源类型/行动类型/选中/警告/禁用），固化到样式与图例。
- 图层与密度：Map 图例改为角落悬浮卡片；当勾选 Trails 时自动缩略长度并提供渐隐；在缩放阈值以下隐藏资源条形图。
- Inspector 深化：按 docs/architecture/INSPECTOR_PANEL.md 接入人格/Traits 与历史趋势（最小 32 帧环形缓冲）。
- 搜索与收藏：Inspector 列表增加星标收藏分组；搜索支持 	ype:agent tag:alpha 语法（可后续）。

## 具体改动建议（与代码位置）
- Toolbar：在 AppHost::drawMainMenuBar 后追加一行工具条组件，抽取现有 Welcome 面板内的播放/步进/倍速控件（AppHost.cpp:540-575）。
- World Generation：
  - 默认 ImGui::CollapsingHeader("命令队列状态")；新增过滤与“清除已完成”（遍历 RuntimeBridge::commandStatusSnapshot()，在 GUI 侧移除已完成的本地显示项）。
  - 成功/失败后调用统一通知接口（新增 pushToast(message, level)）。
- Log Console：增加“级别过滤/搜索/清空”与 F8 快捷键；默认放到底部 Dock。
- Map View：
  - 标签隐藏阈值：根据缩放与边界计算文本遮挡，低于阈值不绘制。
  - 选中/高亮：加粗描边和环形强调，统一到 highlightColor 常量。
  - 交互提示：首次进入/一段时间未操作时在角落绘制手势提示。
- Inspector：将按钮排布至标题行；Follow 切换时同步 scene_selected_node_；增加“复制快照 JSON”。

## 验收标准
- 单屏 1920×1080 下，Map/Scene 主视图≥60% 面积；Inspector 常驻下仍可清晰阅读地图与文本。
- 常用操作（暂停/步进/定位/跟随）≤1 次点击可达，且有可见反馈（toast/高亮）。
- 文案统一，无明显截断；窗口布局可一键恢复默认预设。
- 运行 30 分钟内无 UI 抖动或明显帧率下降；命令失败有红色提示并能在日志中定位。

## 待确认问题（需要你的决策）
- 语言策略：是否完全中文化，还是保留中英文切换？默认语言选择？
- 目标用户画像：仅面向开发者调试，还是也服务关卡/内容编辑？这会影响面板优先级与术语颗粒度。
- 默认布局：你更偏好 Map 为主（俯视大图）还是 Scene 为主（室内/局部）？
- 快捷键约定：是否启用一组全局快捷键（F5 播放/暂停、F6 单步、F7×10、F8 日志、F9 Telemetry）？

## 后续输出
- 合并上述 Phase 0 变更后，补充 docs/architecture/SANDBOX_GUI.md 的“交互与布局”章节，并在 docs/status/todo.md 跟踪实现条目。

## 决策同步（确认采纳）
- 语言：当前阶段统一英文（后续引入 i18n）。
- 默认视图：以 Map 为主，Scene 为辅（Tab 切换）。
- 快捷键：F5（Pause/Resume）, F6（Step×1）, F7（Step×10）, F8（Toggle Log）, F9（Toggle Telemetry）。
- 通知：右上角 toast（3s消退，可堆叠）。
- Map View：节点内加入简易字体图标（按节点类型 R/B/r/·），悬浮 tooltip 展示详情；名称文本发生重叠时自动抑制（重叠或未聚焦时隐藏）。
## 实施记录（2025-10-20）
- AppHost/RuntimeBridge 主要 UI 文本统一为英文，命令状态、提示与错误消息均使用英文表述。
- Map View 默认不再绘制全量拓扑连线；新增节点选择状态，可在地图或 Inspector 中选中节点后显示父节点（金色）与子节点（蓝色）的连线，并支持右键清除选择。
- Inspector “Focus on Map” 与列表选项同步更新地图选择，使边缘联动与高亮一致。节点图标、标签避让与 tooltip 逻辑保持可读。
