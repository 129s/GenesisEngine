# Sandbox GUI：详情卡片顶部实际留白调校（2025-10-22）

问题：仅设置 `WindowPadding=(4,4)` 时，“详情”标题仍贴边，原因是文字上沿超出基线，实际可见留白不足。

调整：
- 将子窗口 `WindowPadding` 改为 `(4,0)`，并在内容开始前手动下移 4px，确保视觉上真正出现 4px 顶部间距；
- 保留左侧 4px `Indent`，底部增加 4px `Dummy`，维持上下对称。

代码：`src/apps/sandbox_gui/gui/ui/BrowserView.cpp` 的 `drawDetailCard`。
