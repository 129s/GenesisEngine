# Sandbox GUI Browser 功能快照（2025-10-23）

- 顶部提供 Scene / World / Monitor / Layouts & Themes 四个分区按钮，点击后会同步切换 `UiState::browser_active_section` 与主视图标签，非 Scene 分区会自动清空搜索缓冲，确保状态一致（参见 `src/sandbox/gui/ui/BrowserView.cpp:59`、`src/sandbox/gui/ui/BrowserView.cpp:84`）。
- **Scene 分区**：支持输入框实时过滤节点名称或 ID，并可一键清除查询；以世界图谱构建可折叠树，自动排序并记忆展开状态，命中搜索的节点会高亮显示；点击节点会触发 Scene/Map 聚焦与 Inspector 选中同步（`src/sandbox/gui/ui/BrowserView.cpp:121`、`src/sandbox/gui/ui/BrowserView.cpp:133`、`src/sandbox/gui/ui/BrowserView.cpp:175`、`src/sandbox/gui/ui/BrowserView.cpp:311`）。
- Scene 树在节点下方追加 Agents 与 Resources 子列表，数据源来自最新快照；选择条目将刷新 Inspector 选中项、节点高亮与 Map 选中位置，并在跟随模式下强制 Scene 聚焦（`src/sandbox/gui/ui/BrowserView.cpp:349`、`src/sandbox/gui/ui/BrowserView.cpp:369`、`src/sandbox/gui/ui/BrowserView.cpp:406`）。
- **World 分区**：展示最近的世界生成/加载/保存状态，并可快速跳转至 World 主视图；若 Runtime 提供最后一次生成记录，还会显示成功标记、种子与错误信息（`src/sandbox/gui/ui/BrowserView.cpp:445`）。
- **Monitor 分区**：显示 UI FPS、命令队列长度与事件数量，可一键切换到 Monitor 面板；当缺少快照时提供提示（`src/sandbox/gui/ui/BrowserView.cpp:480`）。
- **Layouts & Themes 分区**：当前提供布局/主题功能预告与 Settings 面板跳转按钮，为后续导入导出能力预留空间（`src/sandbox/gui/ui/BrowserView.cpp:505`）。
- 各分区的跳转按钮都会更新 `UiState::main_view_active_tab`，确保 Browser 选择与主面板联动；整个 Browser 视图基于 ImGui 渲染并复用 `DesignTokens` 统一样式。
