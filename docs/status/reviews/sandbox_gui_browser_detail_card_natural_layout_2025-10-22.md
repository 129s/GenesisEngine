# Sandbox GUI：Browser 详情卡片“自然布局”临时切换（2025-10-22）

按用户要求，为了观察最“自然”的效果，临时移除了详情卡片中的所有手工边距与换行计算：
- 不再覆盖 `WindowPadding`/`ItemSpacing`；
- 取消 `Dummy/Indent` 等人为留白；
- 取消自定义 `PushTextWrapPos`，仅保留 `Text`/`TextWrapped` 的默认行为；
- 警告区仅保留简单的 `Bullet + TextWrapped`；
- 其它字段保持原有输出顺序与逻辑。

代码：`src/apps/sandbox_gui/gui/ui/BrowserView.cpp` 的 `drawDetailCard` 重写为极简版本。
构建：Release 构建通过。

后续：根据你对“自然布局”观感的反馈，再反向加回必要的、规则化的边距与分隔，形成明确的版式规范。
