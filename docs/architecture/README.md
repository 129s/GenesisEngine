# GenesisEngine · 架构文档索引

> 面向内部开发者，聚焦运行时契约、世界模型与 Agent 行为设计。愿景与原则见 [meta/vision.md](meta/vision.md)，统一路线图见 `../roadmap/README.md`。

## 阅读导航
- **Meta / Blueprint**
  - [meta/genesis-engine-rearchitecture.md](meta/genesis-engine-rearchitecture.md)：重构蓝图、模块拆分与路线图。
  - [meta/namespace-strategy.md](meta/namespace-strategy.md)：命名空间统一规范与迁移步骤。
  - [meta/vision.md](meta/vision.md)：长期愿景、设计原则与成功标准。
- **Foundation**
  - [foundation/architecture-overview.md](foundation/architecture-overview.md)：分层结构、运行循环、并发/数据流边界。
  - [foundation/runtime-api.md](foundation/runtime-api.md)：运行时控制/查询协议、Telemetry/WorldAtlas 契约。
- **World Layer**
  - [world/world-model.md](world/world-model.md)：Map 图（有向）+ Scene 分组 + Interaction/Portal 与坐标契约。
  - [world/world-generation.md](world/world-generation.md)：生成流程（maps/map_edges 与每图 scenes/interactions/portals）、校验与产出。
  - [world/world-representation.md](world/world-representation.md)：渲染表示（Atlas/Tilemap 仅表现）、与运行时解耦的消费准则。
  - [world/chunked-tile-graph.md](world/chunked-tile-graph.md)：大地图/分块加载探索笔记。
- **Agents / Behavior Layer**
  - [agents/agent-personality-big5.md](agents/agent-personality-big5.md)：属性→需求→动机→行动链路与人格/Traits 调制。
- **Interface & Rendering**
  - Sandbox GUI：[`interface/sandbox/sandbox-gui.md`](interface/sandbox/sandbox-gui.md)、[`sandbox-gui-sim-loop.md`](interface/sandbox/sandbox-gui-sim-loop.md)、[`sandbox-gui-tilemap-rendering.md`](interface/sandbox/sandbox-gui-tilemap-rendering.md)、[`sandbox-gui-scene-unification.md`](interface/sandbox/sandbox-gui-scene-unification.md)、[`sandbox-gui-ux-redesign.md`](interface/sandbox/sandbox-gui-ux-redesign.md)、[`inspector-panel.md`](interface/sandbox/inspector-panel.md)。
  - 渲染体系：[`interface/rendering/rendering-modes.md`](interface/rendering/rendering-modes.md)、[`interface/rendering/zero-asset-rendering.md`](interface/rendering/zero-asset-rendering.md)；更多细节见 `../rendering/README.md`。
  - CLI（历史参考）：[`archive/graph-to-grid.md`](archive/graph-to-grid.md)。
- **Backlog**
  - [`backlog/open-questions.md`](backlog/open-questions.md)：仍待决策的议题与假设清单。

## 维护约定
- 新协议/数据结构发布时，请同步更新对应文档并在提交信息中引用。
- 保持术语一致：Scene/Interactive、Global/Local Tile Coordinates、Need/Motive/Action 等关键概念应与代码实现对齐。
- CLI 相关文档仅保留存量知识与排错指南；主力前端为 GUI。
- 文档默认使用中文描述，第一次出现的英文名词需给出解释或中英文并列。
