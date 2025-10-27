# Sandbox GUI：World Tab 布局重构纪要（2025-10-22）

## 调整目标
- 在全局样式归零后，为 World 面板恢复整齐、易读的纵向节奏。
- 将命令控制拆分为多个卡片，保持输入控件与状态文字的对齐与留白一致。

## 主要改动
- 新增 `Style::Layout::CardScope`、`WindowStyleScope` 等助手（`src/apps/sandbox_gui/include/sandbox/gui/style/LayoutMetrics.hpp`），集中处理窗口、卡片、顶栏的 padding/spacing。
- World Tab 重新拆分为三张卡片：
  1. **世界生成**：配置路径、输出路径、随机/固定种子、生成按钮、最近结果与日志。
  2. **加载/保存**：独立的输入区域与按钮，并显示状态文案。
  3. **命令脚本**：脚本输入与执行反馈。
- 卡片内的输入控件改为“标题 + 全宽输入框”样式，所有间距使用 `CardLayoutConfig` 提供的 `headerGap/sectionGap/lineGap`。
- 生成/加载/保存按钮会根据运行时状态与必填字段自动禁用，并配合高亮的警示文字。
- Main View 外层使用 `WindowStyleScope` 统一窗口 padding 与元素间距，避免不同标签出现节奏差异。

## 影响
- World 面板现以卡片形式呈现，整体上下留白一致，文本不会贴边。
- 复用 `LayoutMetrics` 的其它面板（Browser、StatusBar、ControlBar、MainView）同步受益，可继续在 Inspector/Monitor 中推广。

## 验证
- Release 构建与 `ctest -C Release` 全量通过。

## 后续建议
- 为命令列表、Atlas 概览等余下区块引入卡片/表格化布局，进一步统一视觉结构。
- 将 `CardLayoutConfig` 拆分为多种预设（例如紧凑型/信息型），以适配不同面板需求。
