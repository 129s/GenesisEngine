# Sandbox GUI：详情卡片左侧仍显“贴边”的处理补充（2025-10-22）

部分环境下 4px WindowPadding 视觉不显著。为确保左侧不再贴边：
- `BeginChild(..., ImGuiWindowFlags_AlwaysUseWindowPadding)` 强制子窗口使用设定的 WindowPadding；
- 在子窗口内部增加 `Indent(4.0f)`/`Unindent(4.0f)`，与 WindowPadding 一致，保证最小 4px 左侧缓冲可见。

代码：`src/apps/sandbox_gui/gui/ui/BrowserView.cpp` 中 `drawDetailCard`。
