# Sandbox GUI 配色方案（Serum 2 取样）

> 参考 Xfer Serum 2 界面（`build/src/serum.png`），统一 Sandbox GUI 的冷色系基调与交互状态。色值由脚本对目标区域取样（均值 + 亮度调整）得到，以 sRGB 十六进制记录，并在 `DesignTokens` 中换算为 0–1 浮点。

## 色板总览

| Token | Hex | 用途 | 备注 |
| --- | --- | --- | --- |
| `Canvas` | `#191D21` | 全局背景、Dock 空白 | 取自顶部仪表板暗区 |
| `Surface` | `#20262B` | 一级面板、Child 窗口 | 主内容区大面积底色 |
| `SurfaceAlt` | `#23292E` | 次级容器、分组背景 | 表格/列表常态底色 |
| `SurfaceActive` | `#272C32` | Hover 时的底层提升 | 按钮/树节点 hover 背景 |
| `Primary` | `#32C3FF` | 主要操作按钮默认态 | 取自路径编辑按钮 |
| `PrimaryHover` | `#51CCFF` | 主要操作 hover | 亮度 +10% 的 Primary |
| `PrimaryActive` | `#00B4FF` | 主要操作按下/选中 | 波形面板明亮蓝线 |
| `Accent` | `#40B5A0` | 二级强调、描边 | 旋钮内圈青绿色 |
| `AccentHover` | `#4DC0AC` | 二级强调 hover | Accent 提升 12% |
| `AccentActive` | `#369A88` | 二级强调 active | Accent 压低亮度后的色值 |
| `TextPrimary` | `#B1C5CC` | 主体文字 | 顶部标签与信息文本 |
| `TextSecondary` | `#8193A1` | 次级文字/说明 | 滑块数值说明 |
| `TextDisabled` | `#627079` | 禁用文字 | 次级按钮/禁用指标 |
| `BorderSoft` | `#2F393D` | 默认描边/分隔线 | 分割线及卡片描边 |
| `BorderStrong` | `#08090A` | 强分割线/焦点描边 | 取自顶栏黑色线条 |
| `Success` | `#6CFF00` | 成功状态、指标高亮 | OSC 绿灯，与叶子条目渐变共用 |
| `Warning` | `#FFC857` | 警告提示 | 与冷背景仍保持可读 |
| `Danger` | `#FF6B6B` | 错误/危险 | 禁止与 Toast 错误态 |
| `Info` | `#32C3FF` | 信息提示/路径强调 | 与 Primary 同调，避免费色漂移 |
| `Muted` | `#293234` | 次级表面/禁用背景 | 表格交替底色、禁用控件 |
| `Highlight` | `#DAEFFF` | 特殊高亮/指示线 | 取自下方钢琴键高光 |

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
