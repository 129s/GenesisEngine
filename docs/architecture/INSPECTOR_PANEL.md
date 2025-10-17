# Sandbox GUI · Inspector 面板（前置调试）

## 目标
- 提供一个统一的实体观察与调试入口，支持选中/跟随、字段查看与基本高亮，服务于人格/需求/行动等系统的验证与调参。

## 功能范围（MVP）
- 实体列表与搜索：按类型（Agent/Resource/Location）分组，支持名称/ID 过滤。
- 选中与跟随：
  - 选中后在 World View 高亮对应节点/代理；
  - 提供“Follow”开关，World View/Tile View 视角可追随被选实体（后续 Tile View 接入）。
- 详情展示（Agent）：
  - 标识：`entityId`、`name`；
  - 人格（大五 OCEAN）：O/C/E/A/N；
  - Needs：各 Need 当前值/阈值（Hunger/Energy/Social…）；
  - 行动队列：当前行动、队列长度、目标位置、参数（速度/资源/数量/缓解值等）；
  - 位置：当前 LocationId、上次/下次目标；
  - 近期日志/事件摘要（可选）。
- 详情展示（Resource/Location）：名称、库存/容量、位置、关联边，等基础信息。

## 数据来源
- Telemetry：Tick 级快照作为只读来源；必要时扩充 AgentSnapshot 含 `name` 字段；
- Atlas：位置/拓扑仅通过 WorldAtlas 读取（避免跨线程访问 ECS）。

## 交互与集成
- 与 World View 联动：
  - 选中实体时，高亮对应节点/代理；
  - “Follow” 时相机定位到对应节点/代理（仅平移，不缩放）。
- 与 Tile View（后续）联动：
  - 若实体处于具备 Tilemap 的节点，Inspector 支持一键跳转到 Tile View。

## 性能与并发
- UI 线程仅消费最新快照，不跨线程访问 ECS。
- 列表/搜索在 UI 端做轻量缓存/索引，避免每帧全量重组。

## 验收
- 能列出、检索并选中 3 个 Demo Agents；
- 能显示其 OCEAN、Needs、当前行动与目标、所在 LocationId；
- World View 可高亮并可选择跟随；
- 运行 30 分钟稳定无错误。

