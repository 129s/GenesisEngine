# Sandbox GUI：Monitor 面板卡片化重构（2025-10-22）

## 背景
全局样式归零后，Monitor 标签仅保留原始 Child 布局，标题/分隔线与内容间距不一致。为保持界面整齐，本次将 Monitor 视图改为与 World / Inspector 相同的卡片结构。

## 实施
- 使用 `Style::Layout::CardScope` 为 Monitor 标签创建两张卡片：
  1. **运行概览**：展示基础指标、需求统计、资源监控表。
  2. **运行日志**：包含自动滚动开关与固定高度滚动区域。
- 新增分节标题（`CardSectionHeader`）用于“基础数据”“需求统计”“资源监控”“控制”“日志流”等段落。
- 资源监控改为三列表格：节点 ID、资源名、库存。
- 日志滚动区域设定最小高度 180px，并保持自动滚动逻辑。

## 代码
- `src/apps/sandbox_gui/gui/ui/MainView.cpp`: `drawMonitorTab`、`drawMonitorTelemetry`、`drawMonitorLog` 重写；引入卡片布局。
- `src/apps/sandbox_gui/include/sandbox/gui/ui/MainView.hpp`: 更新函数签名，添加 `LayoutMetrics` 依赖。

## 验证
- Release 构建通过。
- `ctest -C Release --output-on-failure -j 4` 通过。

## 后续
- Monitor 资源表后续可扩展排序/筛选。
- 视图组件（卡片、列表、表格）可沉淀为 `LayoutMetrics` 的独立 helper，供其它面板调用。
