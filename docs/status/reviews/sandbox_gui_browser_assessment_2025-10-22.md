# Sandbox GUI Browser 现状评估（2025-10-22）

> **2025-10-24 更新**：Browser 面板已改为 `data/` 目录资源树，原评估中的分区/节点树问题大部分已解决；仍保留本文档以记录改造前的痛点与设计决策。

## 背景
- 目标：为即将开展的 sandbox-gui 重构梳理 Browser 面板的现存问题。
- 依据：`src/apps/sandbox_gui/gui/ui/BrowserView.cpp` 以及相关 UI 状态/上下文定义。

## 主要发现
1. **体量过大且缺乏分层**  
   - `BrowserView::render` 单函数承担全部渲染、状态同步、数据转换逻辑（约 500 行，`src/apps/sandbox_gui/gui/ui/BrowserView.cpp:42` 起）。  
   - 场景树、监控摘要、世界信息、主题占位均写在同一层级，缺少独立子组件或 presenter，难以维护与测试。

2. **渲染过程重复构建昂贵数据结构**  
   - 每帧都会重建节点树映射与排序（`childrenMap` 等，`src/apps/sandbox_gui/gui/ui/BrowserView.cpp:175` 起），即便场景未发生变化仍有 O(n log n) 的开销。  
   - 代理与资源的索引映射也在每帧遍历快照并填充 `unordered_map`（`agentsByNode`/`resourcesByNode`，`src/apps/sandbox_gui/gui/ui/BrowserView.cpp:244` 起），规模稍大即可拖慢 UI。

3. **搜索实现缺乏缓存与扩展性**  
   - 搜索过程中为每个节点重新生成小写字符串（`toLowerCopy`），无法复用结果；若节点量大或频繁输入，CPU/分配成本明显。  
   - 搜索仅匹配节点名称/ID，无法过滤代理、资源等实体，也没有结果概览或跳转能力。

4. **与其他视图耦合紧密但交互有限**  
   - Browser 直接写入多个 `UiState` 字段（主视图标签、Inspector 选中项等），导致其与 Main View、Inspector 的耦合度高。  
   - 然而 World/Monitor/Layouts 分区仅提供跳转按钮或静态文案（如「打开世界面板」，`src/apps/sandbox_gui/gui/ui/BrowserView.cpp:459`；「最近生成」仍以英文 Success/Failed 展示，`src/apps/sandbox_gui/gui/ui/BrowserView.cpp:470`），缺乏真正的浏览/概览能力。

5. **用户体验上的一致性与规模化问题**  
   - 场景树下代理/资源清单一次性展开全部条目，数量较大时难以浏览，也没有分组、分页或排序。  
   - 中文与英文文案混用（如「节点」「Agents」「Success/Failed」），显得割裂；没有统一的设计令牌或组件复用（与 MainView 内的按钮样式函数重复）。
   - 世界切换、电 Atlus 重新加载时并未清理缓存展开节点，存在指向过期 node id 的风险。

## 初步建议
1. 拆分 `BrowserView` 为按分区或职责划分的子视图，并为场景树引入专门的 presenter/state 更新入口。  
2. 将场景树、代理/资源索引改为增量更新：监听 atlas/snapshot 变化后再重建缓存，帧渲染仅消费缓存。  
3. 扩展搜索/筛选能力，至少涵盖代理、资源以及跳转定位，并统一结果高亮/滚动行为。  
4. 重新定义 World/Monitor/Layouts 分区的角色：若侧重概览，应在此提供关键指标与操作，不仅仅是跳转按钮。  
5. 引入共享的 UI 助手（例如按钮状态样式、文本本地化），并在世界重载等事件时清理 `browser_scene_expanded_nodes` 等状态。
