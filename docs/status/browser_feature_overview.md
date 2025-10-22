# Sandbox GUI Browser 功能快照（2025-10-23）

- 顶栏现包含 All / Scene / World / Monitor / Layouts & Themes 五个按钮，仅更新 `UiState::browser_active_section`，默认选中 All，从而彻底解耦 Browser 与主视图标签（`src/sandbox/gui/ui/BrowserView.cpp:60`、`src/sandbox/gui/ui/BrowserView.cpp:79`、`include/sandbox/gui/ui/UiState.hpp:56`）。
- 搜索框在任何分区都常驻，支持按名称或 ID 过滤 Scene 节点，可快速清除；同一查询会同步作用于 Scene 分区及 All 分区内的 Scene 区块（`src/sandbox/gui/ui/BrowserView.cpp:112`-`src/sandbox/gui/ui/BrowserView.cpp:141`）。
- Scene 区块继续构建可折叠树，保持排序与展开记忆；命中项高亮，点击只同步 Scene / Map / Inspector 状态，不再强制切换主视图标签（`src/sandbox/gui/ui/BrowserView.cpp:162`-`src/sandbox/gui/ui/BrowserView.cpp:325`）。
- 节点下方仍展示 Agents / Resources 子列表，依赖最新快照同步 Inspector 选中与跟随定位体验（`src/sandbox/gui/ui/BrowserView.cpp:362`-`src/sandbox/gui/ui/BrowserView.cpp:412`）。
- All 分区串联 Scene 树、世界状态、运行监控及主题提示，复用同一渲染函数让用户在单页浏览所有内容（`src/sandbox/gui/ui/BrowserView.cpp:521`-`src/sandbox/gui/ui/BrowserView.cpp:547`）。
- World / Monitor / Layouts & Themes 板块的快捷按钮仅修改主视图标签，不再改写 Browser 状态，同时保留最新状态与错误提示（`src/sandbox/gui/ui/BrowserView.cpp:465`、`src/sandbox/gui/ui/BrowserView.cpp:502`、`src/sandbox/gui/ui/BrowserView.cpp:513`）。
