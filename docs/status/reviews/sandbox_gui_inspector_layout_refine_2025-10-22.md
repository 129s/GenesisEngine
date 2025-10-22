# Sandbox GUI：Inspector 卡片化重构（2025-10-22）

## 调整摘要
- Inspector 窗口应用统一 `WindowStyleScope`/`CardScope` 布局，确保窗口内边距与文本换行一致。
- 针对 Agent / Resource / Node 三类选择对象引入分节标题（CardSectionHeader），采用统一的段落留白与分隔线。
- Agent 详情：增加“需求概览”“当前行动”“Planner 决策”“移动进度”“本帧需求变更”等分节，表格与文案遵循统一节奏。
- Resource 详情：新增“资源概览”“关联生成点”“活跃消耗者”卡片段落，展示 spawn/consumers 列表并同步 JSON 按钮输出。
- Node 详情：整合“节点信息”“子节点”“关联边”“资源”“在此节点的 Agent”详表，并保留 JSON 导出。
- 运行事件日志作为独立分节呈现，统一滚动区域高度。

## 影响
- Inspector 面板从零内边距状态恢复为规整布局，字段对齐、分节间距和按钮排布均统一。
- JSON 复制结果同步复用了新的数据聚合逻辑，避免重复遍历。

## 验证
- Release 构建通过。
- `ctest -C Release --output-on-failure -j 4` 全量成功。

## 后续建议
- 将资源/节点的表格进一步细化为可排序/可折叠结构。
- 结合 LayoutMetrics 扩展更多通用组件（如 KeyValue 行、列表卡片），供其它面板复用。
