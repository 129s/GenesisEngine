# Sandbox GUI · Inspector 面板

## 目标
- 为内部开发者提供统一的实体观察与调试入口，支持选中/跟随、字段查看与高亮，覆盖需求→决策→行动链路；Traits/显式 Attributes/属性映射属于 proposal。

## 当前实现（2025-10-19）
- 实体列表：按 Agent / Resource / Node 三类展示，依托 Browser 负责筛选/搜索；选中后自动高亮对应地图节点。
- 详情面板：
  - Agent：显示 ID、名称、所在节点、需求强度、需求临界标记、当前行动/Planner 结果、移动进度；支持“定位到地图”“打开 Scene”以及跟随模式（同步 SceneView 节点）。
  - Resource：显示所属节点、类型与库存；支持一键定位。
  - Node：显示 ID、父节点、类型并可跳转。
- 事件与差异：展示本帧 Runtime 事件日志，并基于 `SimulationSnapshotDiff` 列出所选 Agent 的需求变化。
- 地图联动：MapView 以描边高亮所选节点/Agent，Inspector 面板中跟随开关会驱动 SceneView 聚焦节点。
- 小地图：Inspector 顶部嵌入 Scene MiniMap，复用 Scene 视口状态，默认展现当前聚焦节点，并遵循 Inspector 开关一起显示/隐藏；卡片左上内边距已移除以便与小地图完全对齐。
- 快照导出：Agent / Resource / Node 详情提供 “Copy JSON” 按钮，复制结构化快照（含 Telemetry / Atlas 上下文），便于日志分析与回归测试。

## 待完善清单
- Agent 详情需补充最近日志与更细粒度的行动/生产信息；人格 Big5（core 已落地但 GUI 未展示）、Traits、属性映射属于 proposal。
- Resource 需展示消耗历史与再生速率趋势。
- Portal/Scene 详情尚未落地。
- （改由 Browser 实现的）搜索与标签过滤迁移完成，Inspector 后续聚焦编辑与详情强化。

## 功能范围（MVP）
- **实体列表**：按类型（Agent / Scene / Interactive:Resource / Interactive:Portal）分组展示；名称、ID、Trait、mapId 等过滤统一在 Browser 侧实现。
- **选中与跟随**
  - MapView 高亮对应节点/代理，显示 Portal 方向箭头。
  - SceneView 若存在 insideView，则自动切换并聚焦代理；Follow 开关可在两视图中同步追踪（默认仅平移）。
- **Agent 详情**
  - 基本信息：`entityId`、`name`、`mapId`、`position(x,y)`、当前交互点/Portal（若有）。
  - 需求：值、阈值与临界标记、最近一次满足时间（若可得）。
  - 人格（Big5）：core 已落地；Inspector 展示待补（见 `docs/architecture/agents/agent-model-v0.md`）。
  - Traits：Proposal（未落地，见 `docs/architecture/proposals/agents/agent-personality-big5.md`）。
  - 行动队列：当前行动、目标节点（Portal/交互点）、剩余步数、计划长度。
  - 当前目标链：Need → Planner 结果 → 执行动作摘要。
  - 近期日志（可选）：最近 N 步的需求/行动变更。
- **Resource / Portal / Scene 详情**
  - 资源：类型、库存/容量、再生速率、所属 Scene、`coord_local/coord_global`、消费记录。
  - Portal：起止 Scene、`anchors{at_from, at_to}`、通行掩码、目标 Interactive。
  - Scene：父 Scene、布局描述（layout）、子节点汇总、Portal/资源统计。

## 数据来源
- Telemetry：`agents[]`、`needs[]`、`actions[]`、`resources[]`、`plannerDecisions[]`、`workshopAttempts[]`、`resourceAttempts[]`。
- WorldAtlas：节点层级、Portal、Tilemap 元数据。Inspector 严禁跨线程访问 ECS。
- 历史缓冲：UI 端维护小型 ring buffer（例如 64 帧）跟踪需求强度变化，便于趋势展示。

## 交互与集成
- MapView 联动：选中实体后高亮节点/边；支持“一键跳到父 District/Region”以快速定位。
- SceneView 联动：若实体位于可渲染 map，则 SceneView 聚焦锚点；Portal 面板可触发 SceneView 显示目标入口。
- 调试信息导出：提供“复制快照”按钮输出 JSON 片段，供日志或测试使用。

## 性能与并发
- Inspector 仅消费最新快照，不跨线程访问 Runtime 数据结构。
- 列表使用 UI 端缓存，按需增量更新，避免每帧全量重建。
- 对高频刷新字段（需求强度、属性值）使用环形历史缓冲，防止内存膨胀。

## 验收标准
- 可列出并检索 Demo 代理，准确显示人格/Traits、属性、需求、当前目标与行动队列。
- MapView/SceneView 跟随与高亮工作正常；Follow 模式下视图同步。
- 长时间运行（≥30 分钟）无 UI 卡顿或资源泄漏。
- 资源/Portal 面板展示数据与 WorldAtlas 一致。
