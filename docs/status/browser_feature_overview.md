# Sandbox GUI Browser 功能快照（2025-10-24）

- Browser 已转为“资源管理器”模式，直接映射 `data/` 目录结构：目录按字母排序置顶，文件紧随其后（`src/sandbox/gui/ui/BrowserView.cpp:167`-`src/sandbox/gui/ui/BrowserView.cpp:210`）。
- 顶部保留单一搜索框，支持对目录/文件名与相对路径的模糊匹配；命中项自动高亮并展开父目录，清除按钮即时复位（`src/sandbox/gui/ui/BrowserView.cpp:305`-`src/sandbox/gui/ui/BrowserView.cpp:347`）。
- 展开状态现在以相对路径持久化（`UiState::browser_expanded_paths`），默认保持根目录 `.` 打开；所选条目同样记录为相对路径，刷新后仍能聚焦（`include/sandbox/gui/ui/UiState.hpp:69`-`include/sandbox/gui/ui/UiState.hpp:71`）。
- 面板下半部新增详情区，展示所选条目的相对/绝对路径、类型、大小与最后修改时间，便于快速核对生成资产（`src/sandbox/gui/ui/BrowserView.cpp:369`-`src/sandbox/gui/ui/BrowserView.cpp:409`）。
- 数据目录定位统一通过 `locateAsset("data")`，兼容可写构建目录与发布包结构；若目录缺失会给出高亮提示（`src/sandbox/gui/ui/BrowserView.cpp:255`-`src/sandbox/gui/ui/BrowserView.cpp:284`）。
