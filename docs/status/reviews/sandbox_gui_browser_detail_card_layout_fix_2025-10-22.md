# Sandbox GUI：Browser 详情卡片布局修复说明（2025-10-22）

本次修复聚焦于详情卡片的边距与标题区垂直节奏。

## 修复项
- 标题区上下留白统一：移除 `Spacing(); Separator(); Spacing();` 的不对称组合，改为分隔线前后各 `blockSpacing` 占位，视觉上“详情”上下边距一致。
- 左右包边统一：取消 `PushTextWrapPos(... - outerPadding)` 的额外右侧收缩，改为对齐子窗口可用宽度，确保左右内边距一致（由 `WindowPadding` 控制）。

## 涉及代码
- `src/sandbox/gui/ui/BrowserView.cpp`：函数 `drawDetailCard` 内部布局参数与分隔符周围留白调整。

## 影响范围
- 仅影响 Browser 面板下方“详情”卡片的文本排版与留白，未更改业务逻辑。
- 经过 Release 构建通过（本地 `cmake --build build --config Release`）。

## 后续建议
- 如需更强的区块一致性，可提取 `SectionHeader(title)` 帮助函数，统一标题、分隔线与留白策略，复用于其它面板。
