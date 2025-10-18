# Sandbox GUI · Inspector 面板

## 目标
- 为内部开发者提供统一的实体观察与调试入口，支持选中/跟随、字段查看与高亮，覆盖属性→需求→决策链路以及人格/Traits 的调试需求。

## 功能范围（MVP）
- **实体列表与搜索**：按类型（Agent/Resource/Portal/Area）分组，可按名称、ID、Trait、mapId 过滤。
- **选中与跟随**
  - MapView 高亮对应节点/代理，显示 Portal 方向箭头。
  - SceneView 若存在 insideView，则自动切换并聚焦代理；Follow 开关可在两视图中同步追踪（默认仅平移）。
- **Agent 详情**
  - 基本信息：`entityId`、`name`、`mapId`、`localPosition`、当前区域/Portal。
  - 属性：health/sanity/hunger/energy 等当前值、衰减率。
  - 需求：强度、阈值、准备裕度、人格/Traits 权重贡献、最近一次满足时间。
  - 人格与 Traits：OCEAN 五维、Trait 列表及描述。
  - 行动队列：当前行动、目标节点（Portal/交互点）、剩余步数、计划长度。
  - 当前目标链：Need → Planner 结果 → 执行动作摘要。
  - 近期日志（可选）：最近 N 步的需求/行动变更。
- **Resource / Portal / Area 详情**
  - 资源：类型、库存/容量、再生速率、所属 map、锚点坐标。
  - Portal：起止 map、锚点、通行掩码、连接的 Area。
  - Area：父 District、锚点集合、交互节点、Portal 汇总。

## 数据来源
- Telemetry：`agents[]`、`attributes[]`、`needs[]`、`actions[]`、`resources[]`、`traits[]`。
- WorldAtlas：节点层级、Portal、Tilemap 元数据。Inspector 严禁跨线程访问 ECS。
- 历史缓冲：UI 端维护小型 ring buffer（例如 64 帧）跟踪需求强度变化，便于趋势展示。

## 交互与集成
- MapView 联动：选中实体后高亮节点/边；支持“一键跳到父 District/Region”以快速定位。
- SceneView 联动：若实体位于可渲染 map，则 SceneView 聚焦锚点；Portal 面板可触发 SceneView 显示目标入口。
- 调试信息导出：提供“复制快照”按钮输出 JSON 片段，供日志或测试使用。

## 性能与并发
- Inspector 仅消费最新快照，不跨线程访问 Runtime 数据结构。
- 列表/搜索使用 UI 端缓存，按需增量更新，避免每帧全量重建。
- 对高频刷新字段（需求强度、属性值）使用环形历史缓冲，防止内存膨胀。

## 验收标准
- 可列出并检索 Demo 代理，准确显示人格/Traits、属性、需求、当前目标与行动队列。
- MapView/SceneView 跟随与高亮工作正常；Follow 模式下视图同步。
- 长时间运行（≥30 分钟）无 UI 卡顿或资源泄漏。
- 资源/Portal 面板展示数据与 WorldAtlas 一致。
