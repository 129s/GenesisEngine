# Sandbox GUI：Browser 详情卡片布局定版记录（2025-10-22）

## 布局策略
- 使用设计令牌：横向 `Md=12px`、纵向 `Sm=8px` 作为统一内边距。
- `ItemSpacing` 设为 `(Xs=4px, Sm=8px)`，保证字段间垂直节奏一致，横向仅保留最小间距。
- 标题与分隔线之间采用对称留白：`Xs` 的上下 `Dummy`。
- 警告段落加入统一缩进（`Sm=8px`）与 `BulletText`，继承 wrap 设置避免断行痕迹。
- `ImGui::PushTextWrapPos(cursor + avail)` 确保文本在可视区域内自然换行，同时配合 `ImGuiWindowFlags_AlwaysUseWindowPadding` 让内边距真实生效。

## 代码位置
- `src/sandbox/gui/ui/BrowserView.cpp:364` 起的 `drawDetailCard` 函数。

## 说明
- 解决问题：标题贴边/裁切、上下不对称、左右无留白等。
- 构建验证：`cmake --build build --config Release`。
- 后续如需在其它面板复用，可抽取 `DetailCardLayout` 帮助函数。
