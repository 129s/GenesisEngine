# Sandbox GUI 配色方案（Serum 2 取样）

> 参考 Xfer Serum 2 界面，统一 Sandbox GUI 的冷色系基调与交互状态。色值以 sRGB 十六进制记录，并在 `DesignTokens` 中以 0–1 浮点配置。

## 色板总览

| Token | Hex | 用途 | 备注 |
| --- | --- | --- | --- |
| `Canvas` | `#0D141D` | 全局背景、Dock 空白 | 最暗背景，保持 UI 层浮起感 |
| `Surface` | `#152030` | 一级面板、Child 窗口 | 与 Canvas 形成 1 级对比 |
| `SurfaceAlt` | `#1C2B3A` | 次级容器、分组背景 | 在列表/树节点默认态使用 |
| `SurfaceActive` | `#23364B` | Hover 时的底层提升 | 适用于分组/按钮 hover 底色 |
| `Primary` | `#32AEEF` | 主要操作按钮默认态 | 大按钮、主要滑块、active 图标 |
| `PrimaryHover` | `#41BDFB` | 主要操作 hover | 相比默认态增亮 8% |
| `PrimaryActive` | `#1E86D0` | 主要操作按下/选中 | 亦作为“聚焦”高亮色 |
| `Accent` | `#4CC5E7` | 二级强调、描边 | 用于链接、装饰线条 |
| `AccentHover` | `#5ED2F1` | 二级强调 hover | 用于细控件 hover |
| `AccentActive` | `#2A9AD4` | 二级强调 active | Secondary 操作按下态 |
| `TextPrimary` | `#EBF3FD` | 主体文字 | 高对比白蓝 |
| `TextSecondary` | `#AFC4DD` | 次级文字/说明 | 比主文本低 30% 对比 |
| `TextDisabled` | `#5D748C` | 禁用文字 | 低对比但仍可辨认 |
| `BorderSoft` | `#213142` | 默认描边/分隔线 | Dock/Panel 细描边 |
| `BorderStrong` | `#111A27` | 强分割线/焦点描边 | 选中状态外围描边 |
| `Success` | `#6FF484` | 成功状态、指标高亮 | Neon 绿，少量使用 |
| `Warning` | `#FFC857` | 警告提示 | 与冷背景仍保持可读 |
| `Danger` | `#FF6B6B` | 错误/危险 | 禁止与 Toast 错误态 |
| `Info` | `#57C9FF` | 信息提示/路径强调 | Toast、路径提示 |
| `Muted` | `#34455C` | 次级表面/禁用背景 | 表格交替底色、禁用控件 |
| `Highlight` | `#9BE0FF` | 特殊高亮/指示线 | 轨迹、动态指示 |

## 列表/树节点状态矩阵（初版）

| 状态 | 背景 | 文本 | 描边/前景 |
| --- | --- | --- | --- |
| 默认 | `SurfaceAlt` | `TextPrimary` | `BorderSoft` |
| Hover | `SurfaceActive` | `TextPrimary` | `Accent` 适度描边 |
| 选中（焦点） | `PrimaryActive` | `TextPrimary` | `BorderStrong` |
| 禁用/过滤隐藏 | `Muted` | `TextDisabled` | 无描边 |

> 叶子节点额外叠加 `Success` 色系渐变，保留“可交互”语义；若后续 UI 需要暗色叶子主题，可在 `colorForLeaf` 中按 `Success` → `Accent` 重新取样。

## 应用约定

- 按钮：`Primary` 系列用于主按钮；`SurfaceAlt + Accent` 用于次级按钮；危急操作采 `Danger`。
- 滑块/旋钮：默认轨道 `SurfaceActive`，活动段 `Primary`，指示点使用 `Highlight` 以保证可视。
- 分隔线/滚动条：常态 `BorderSoft`，Hover 时可过渡到 `AccentHover`。
- Toast/状态栏：信息 → `Info`，成功 → `Success`，警告 → `Warning`，错误 → `Danger`，文字统一 `TextPrimary`。

## 后续重构里程碑

1. **全局主题更新**：`DesignTokens` 已采用新色板，需逐步检查现有控件（Browser、ControlBar、StatusBar）是否仍覆盖局部颜色。
2. **状态统一化**：为按钮/列表/标签等建立共用的状态 helper，避免每处手动 push/pop 颜色。
3. **视觉回归**：完成主要面板迁移后，截取关键界面组成对比，纳入 UI 回归手册。
4. **文档固化**：未来新增控件需在此文档补充案例，并同步截图/标注。
