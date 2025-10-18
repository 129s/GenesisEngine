# GenesisEngine · 架构文档索引

> 面向内部开发者，聚焦运行时契约、世界模型与 Agent 行为设计。愿景与原则见 `VISION.md`，统一路线图见 `../roadmap/README.md`。

## 阅读导航
- **总览**
  - `overview.md`：分层结构、运行循环、并发/数据流边界。
  - `runtime_api.md`：运行时控制/查询协议、Telemetry/WorldAtlas 契约。
- **世界层（World Layer）**
  - `world_model.md`：Scene/Interactive 节点树、坐标体系、运行层与渲染层的契约。
  - `world_generation.md`：Scene 布局描述、数据驱动世界生成流程与校验策略。
  - `world_representation.md`：运行层节点 → 渲染层 Tilemap 的对齐与消费。
  - `chunked_tile_graph.md`：大地图/分块加载的探索笔记。
- **Agent 层（Behavior Layer）**
  - `agent_personality_big5.md`：属性→需求→动机→行动的链路建模，大五人格/Traits 如何调节点驱与阈值。
- **前端层（Presentation Layer）**
  - CLI（遗留调试）：`graph_to_grid.md`
  - GUI：`sandbox_gui.md`、`sandbox_gui_sim_loop.md`、`sandbox_gui_tilemap_rendering.md`、`inspector_panel.md`
- **其他**
  - `open_questions.md`：仍待决策的议题与挂起假设。

## 维护约定
- 新协议/数据结构发布时，请同步更新对应文档并在提交信息中引用。
- 保持术语一致：Scene/Interactive、Global/Local Tile Coordinates、Need/Motive/Action 等关键概念应与代码实现对齐。
- CLI 相关文档仅保留存量知识与排错指南；主力前端为 GUI。
- 文档默认使用中文描述，第一次出现的英文名词需给出解释或中英文并列。
