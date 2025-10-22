# Sandbox GUI：全局样式标准化（2025-10-22）

为彻底停止 ImGui 默认的间距/边框干预，本次将 `DesignTokens::applyTo` 中的全局样式全部归零，由各 UI 模块自行控制具体像素。

## 调整要点
- `WindowPadding / FramePadding / CellPadding / ItemSpacing / ItemInnerSpacing / IndentSpacing` 均置为 `0`。
- 所有边框厚度（Window/Child/Frame/Popup/Tab/Separator/DockingSeparator）和额外触控 padding 统一置零。
- 滚动条尺寸、圆角、颜色表保持不变；局部需要间距的地方应显式调用 `PushStyleVar` 或使用自定义布局助手。

## 影响范围
- 所有新创建的窗口/Child 默认无额外留白、无边框；如果需要统一留白，应在对应视图内手动应用 `DesignTokens::spacing(...)`。
- 现有视图若依赖旧的全局样式，需要补充局部样式调用（已对 Browser 详情卡处理）。

## 代码位置
- `src/sandbox/gui/style/DesignTokens.cpp`

## 后续建议
- 编写公用布局 helper（如 `ApplySectionSpacing`）供多个面板复用，减少手写 `PushStyleVar`。
- 回归检查 StatusBar / ControlBar / Inspector 等面板，按需补充显式间距配置。
