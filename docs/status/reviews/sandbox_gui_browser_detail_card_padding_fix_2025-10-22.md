# Sandbox GUI：Browser 详情卡片 4px 内边距修复（2025-10-22）

用户反馈“自然布局”下标题上沿被裁切。原因：Child 默认无额外顶部缓冲，第一行文本贴近裁剪边界，部分 DPI/对齐情况下会出现上沿裁切。

修复：
- 仅对 `BrowserDetailCard` 子窗口设置 `WindowPadding=(4,4)`，不改变其它布局与换行逻辑。
- 代码：`src/sandbox/gui/ui/BrowserView.cpp` 中 `drawDetailCard`，在 `BeginChild` 前 `PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4,4))`，在 `EndChild` 后 `PopStyleVar()`。

验证：
- Release 构建通过；其余测试不受影响。

后续：
- 如需更大留白或左右与上下不同步，可按统一规范（如 8/12/16）再行调整。
