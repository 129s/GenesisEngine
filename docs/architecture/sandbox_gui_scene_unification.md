# Sandbox GUI · 地图树与单画布展示

> 记录 Scene 标签页最新的渲染逻辑、状态流以及地图树设计，方便后续迭代和调试。

## 1. 核心概念

### 地图树（生成期）
- 世界生成仍产出一棵 *地图树*，节点按照空间层次划分（如城镇 → 建筑 → 房间）。
- Portal 作为地图树中的交互节点，记录本地入口坐标与目标地图 ID，是穿梭的唯一纽带。
- 地图树保留几何信息，方便设计阶段定位模块产出、调试瓦片布局。

### 地图图（运行期）
- 运行时使用 *Map Graph*（节点 = Map 实例，边 = 地图切换成本）驱动寻路与场景切换。
- Portal 只在运行图上体现为“从当前地图到目标地图”的跳转耗时，不再单独建节点。
- Scene 画布只关心当前地图的瓦片与叠加层，跨地图切换依赖 Portal 点击或外部命令。

## 2. UI 现状（2025-10）

- Scene 标签页自上而下分两层：顶层是一整行工具栏，下层将大地图画布、小地图与 Inspector 同行排布。侧栏宽度自适应（约主视图 1/3），在空间不足时收缩至 200px。
- 小地图仅渲染当前视窗中心向外四倍范围的瓦片概略框架，不再提供滚轮缩放、拖拽或视图复位等交互；用于帮助定位主视窗，相机矩形会在其上方高亮。
- Inspector 依旧由 `UiState::show_inspector` 控制显隐，隐藏时侧栏提供“显示检查器”按钮恢复。
- `MainView::drawSceneUnified` 统一处理工具栏、画布与叠加层：
  1. 画布底层绘制瓦片（棋盘底色暂用于调试）；
  2. 叠加传送门、锚点、资源等信息；
  3. 点击 Portal 会切换到目标地图并重置摄像机；
  4. 点击瓦片会记录选中坐标，供 Inspector/调试使用。
- Toolbar 保留 `SceneSelectionTool`（任意/节点/瓦片）并新增“传送门”叠加开关；
- 摄像机状态使用 `scene_cam_*` 字段，支持滚轮缩放、右键平移、重置居中；
- `scene_show_graph` 现用于控制 Portal 显隐（快捷键 `Ctrl+1`），`scene_show_grid/anchors/resources/ruler` 仍按原语义工作。

## 3. 状态与 Presenter 更新

| 字段/结构 | 说明 |
| --- | --- |
| `UiState::scene_camera_node` | 记录当前摄像机聚焦的地图节点，用于节点切换后自动居中。 |
| `UiState::scene_cam_zoom` | 默认 1.0，以瓦片像素大小为基准缩放。 |
| `SceneNodePortal` | ViewModel 中的新结构，暴露 Portal 的目标节点与在瓦片坐标内的 anchor。 |
| `ScenePresenter::buildNodeViewModel` | 负责从 `atlas.tilemaps` 中抽取 Portal 数据并计算瓦片网格。 |

此前的 `SceneUnifiedViewModel` 已删除：渲染直接使用节点 ViewModel，减少重复构建。

## 4. 交互流程

1. **Browser 选中地图节点** → 更新 `scene_selected_node`，Scene 画布加载对应瓦片并居中。
2. **Portal 左键点击**（工具 = 任意/节点）→ 切换到目标地图，重置摄像机并清空瓦片选中。
3. **瓦片左键点击**（工具 = 任意/瓦片）→ 记录 `scene_tile_selection`，供 Inspector/调试。
4. **右键短按** → 清除瓦片选中；右键拖拽 → 平移摄像机。
5. **标尺模式** → 左键设置锚点，右键释放；提示使用瓦片坐标和欧氏距离。

## 5. 渲染参考

| 文件 | 说明 |
| --- | --- |
| `src/sandbox/gui/ui/MainView.cpp:1260` | 单画布渲染主流程、摄像机与交互处理。|
| `include/sandbox/gui/presenter/MainViewPresenters.hpp:64` | `SceneNodePortal`、`SceneNodeDetails` 定义。|
| `src/sandbox/gui/presenter/MainViewPresenters.cpp:222` | 从 `WorldAtlas::Tilemap` 解析 Portal 信息并补充 ViewModel。|
| `src/sandbox/gui/AppHostCore.cpp:508` | Ctrl+1 快捷键切换“传送门”叠加提示文案。|

## 6. TODO

1. **交互点映射**：为 NPC/交互节点补充瓦片坐标（目前仅资源点/Portal），扩展点击语义。
2. **地图树可视化**：Browser 面板展示生成期的地图树与 Portal 链接，支持从树结构直接跳转。
3. **瓦片素材渲染**：接入真实瓦片纹理或 TileSet，替换调试棋盘底色。
4. **运行图联动**：在 Scene 画布内展示目标地图的基础信息（切换耗时、推荐入口等）。
5. **相机记忆策略**：按需决定跨地图时是否保留缩放/偏移或记忆每张地图的最后视角。
