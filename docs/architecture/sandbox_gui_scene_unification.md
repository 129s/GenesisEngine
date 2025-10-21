# Sandbox GUI · Scene 画布统一重构笔记

> 记录单画布改造后的架构要点、状态流以及仍待跟进的事项，方便后续维护与扩展。

## 1. 背景回顾

最初的 Scene 标签页分成 “世界概览” 与 “节点细节” 两套渲染入口，依赖 `SceneViewMode` 在 `drawSceneWorldMap` / `drawSceneNode` 两个函数之间切换。两套视图拥有独立的摄像机、状态、输入处理，导致：

- Browser / Inspector 需要根据 mode 做条件分支，状态同步易错；
- 画布无法同屏呈现节点拓扑与瓦片细节，用户必须频繁切换；
- 额外的布局元素撑高子窗口，产生滚动条与多余空白。

## 2. 当前实现概览（2025-10）

- `MainView::drawSceneUnified` 取代原有 Map / Node 双函数，负责工具栏、主画布以及瓦片浮动 overlay 的统一绘制。
- `ScenePresenter::buildUnifiedViewModel` 组合原来的 Map / Node ViewModel，减少重复状态转换。
- 两类视图数据共用一套摄像机逻辑：世界画布仍使用 `map_zoom` / `map_pan_*`，瓦片 overlay 使用 `scene_cam_zoom` / `scene_cam_offset_*`。
- Inspector / Browser 跳转时仅设置节点 ID 与工具模式；单画布自动响应并绘制高亮，无需再切换 tab 内部模式。

## 3. 状态层调整

| 字段 | 说明 |
| --- | --- |
| `SceneSelectionTool` | 任意 / 节点 / 瓦片。决定左键命中的优先级（节点画布只在工具允许时响应）。|
| `SceneTileSelection` | `nodeId + (tileX, tileY)` 的瓦片选中信息，供 overlay 高亮与 Inspector 联动。|
| `scene_focus_node_request` | 保留，用于在 Browser / Inspector 请求时重新定位世界摄像机。|
| `scene_view_mode` | 已移除。所有状态改为对单画布生效。|

其它历史字段（`scene_selected_node`、`map_selected_node`、`scene_show_grid` 等）保持不变，但现在同时驱动主画布与瓦片 overlay。

## 4. 画布结构

1. **工具栏（顶部单行）**
   - 选择工具按钮（任意 / 节点 / 瓦片）。
   - 世界叠加层开关：节点、资源、实体、轨迹、插值、标尺。
   - 节点列表下拉框（与 Browser 相同的数据源）。
   - 瓦片辅助开关：网格、锚点、节点资源。
   - 重置视图会同时清理世界摄像机与瓦片摄像机偏移/缩放。

2. **主画布（世界拓扑）**
   - 继续使用 atlas 节点坐标，支持缩放、平移、标尺测量。
   - 节点点击遵从工具模式：仅在 “任意/节点” 时更新节点选中。
   - 右键短按清除 `map_selected_node`，右键拖动平移。

3. **瓦片 overlay**
   - 固定在画布右下角，根据画布可用空间自适应尺寸。
   - 维持独立的缩放 / 平移，支持热区裁剪（`PushClipRect`）。
   - 在 “任意/瓦片” 工具下左键点击记录瓦片坐标，同时同步 `scene_selected_node`。
   - 悬停显示瓦片坐标，选中瓦片以强调色描边。

## 5. 交互联动

- **Browser 点击节点**：设置 `scene_selected_node`、`map_selected_node`、`scene_selection_tool = Node`，同时触发 `scene_focus_node_request`。
- **Inspector 操作**：
  - “定位地图” 将工具切至 `Node` 并重置瓦片选中。
  - “打开节点视图” 将工具切至 `Tile`，同时清空旧瓦片选中，便于用户直接在 overlay 中拾取瓦片。
  - Follow 开关更新后会同步节点并清理瓦片选中，避免跨节点残留。
- **画布点击**：
  - 节点命中优先受工具影响；当 overlay 覆盖鼠标时，世界画布不再处理同一事件，避免误选。
  - 瓦片 overlay 点击会同时同步 `scene_selected_node` 和 `map_selected_node`，确保 Inspector 一致。

## 6. 回归检查清单

- [ ] 世界画布无垂直滚动条，重置视图后布局稳定。
- [ ] 工具栏按钮状态与实际交互一致，快捷键仍能切换原有叠加层。
- [ ] Inspector / Browser / 画布之间的节点高亮同步。
- [ ] overlay 缩放、平移、锚点/资源可见性正常。
- [ ] 标尺、实体轨迹、资源进度条等原有功能维持可用。
- [ ] `scene_selection_tool` 切换后，瓦片与节点点击逻辑正确。

## 7. 后续迭代事项

1. **多命中候选菜单**：当前未实现 “点击同一点弹出候选列表” 的功能，仍需根据需求补充（可利用 `ImGui::BeginPopup`）。
2. **键盘快捷键**：考虑为瓦片工具添加快捷键，或与现有叠加层组合。
3. **持久化瓦片摄像机**：根据反馈决定是否在切换节点时保留缩放 / 偏移。
4. **性能 Profiling**：统一画布后应重新 Profile 大地图 + 叠加层同时开启时的帧率。

## 8. 关联文件索引

| 文件 | 描述 |
| --- | --- |
| `include/sandbox/gui/ui/UiState.hpp` | 新增工具与瓦片选中状态字段。|
| `src/sandbox/gui/ui/MainView.cpp` | 工具栏、主画布、瓦片 overlay 的统一渲染与交互实现。|
| `src/sandbox/gui/presenter/MainViewPresenters.cpp` | 提供统一 ViewModel 构建入口。|
| `src/sandbox/gui/ui/BrowserView.cpp` | 浏览器节点点击时更新新状态字段。|
| `docs/architecture/sandbox_gui_ux_redesign.md` | 记录叠加层、快捷键说明，需要与本重构保持同步。|

