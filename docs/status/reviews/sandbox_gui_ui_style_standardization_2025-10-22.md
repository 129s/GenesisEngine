# Sandbox GUI：全局样式标准化（2025-10-22）

为彻底停止 ImGui 默认的间距/边框干预，本次将 `DesignTokens::applyTo` 中的全局样式全部归零，由各 UI 模块自行控制具体像素，并引入统一的布局助手。

## 调整要点
- `WindowPadding / FramePadding / CellPadding / ItemSpacing / ItemInnerSpacing / IndentSpacing` 均置为 `0`。
- 所有边框厚度（Window/Child/Frame/Popup/Tab/Separator/DockingSeparator）和额外触控 padding 统一置零。
- 滚动条尺寸、圆角、颜色表保持不变；局部需要间距的地方应显式调用 `PushStyleVar` 或使用自定义布局助手。

## 影响范围
- 所有新创建的窗口/Child 默认无额外留白、无边框；如果需要统一留白，应在对应视图内手动应用 `DesignTokens::spacing(...)`。
- 新增 `Style::LayoutMetrics`（`include/sandbox/gui/style/LayoutMetrics.hpp`）：提供 `WindowStyleScope`、`CardScope`、`BarScope` 等 RAII 封装，集中管理常见的窗口/详情卡/顶部工具栏间距。
- Browser 详情卡、Status Bar、Control Bar、Main View 已接入上述助手，恢复对齐与可读性。

## 代码位置
- `src/sandbox/gui/style/DesignTokens.cpp`
- `include/sandbox/gui/style/LayoutMetrics.hpp`
- `src/sandbox/gui/style/LayoutMetrics.cpp`

## 后续建议
- 使用 `LayoutMetrics` 中的 helper 扩展 Inspector/Scene 子面板、对话框等，形成统一套件。
- 根据后续视觉规范调整 `LayoutMetrics` 的默认 spacing/token 组合，必要时拆分出多种卡片/窗口风格。
