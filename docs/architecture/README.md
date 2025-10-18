# GenesisEngine · Architecture Docs

> 面向内部开发者，聚焦架构层设计与实现契约。愿景与原则见 `VISION.md`，路线图见 `../roadmap/README.md`。

## 阅读指引
- **核心层**
  - `overview.md`：引擎目标、分层结构、并发边界。
  - `runtime_api.md`：运行时控制/查询协议与 Telemetry、WorldAtlas 契约。
- **世界层**
  - `world_model.md`：分层节点图 + Tilemap（表/里）的运行时抽象。
  - `world_generation.md`：基于 MapConfig 递归生成的世界生成流程。
  - `world_representation.md`：Graph ↔ Tilemap 协同与前端消费。
  - `chunked_tile_graph.md`：噪声大世界/区块流式方案（专题）。
- **Agent 层**
  - `agent_personality_big5.md`：属性→需求→决策→行动链路，大五人格与 Trait 的映射。
- **前端层**
  - CLI（遗留）：`graph_to_grid.md`
  - GUI：`sandbox_gui.md`、`sandbox_gui_sim_loop.md`、`sandbox_gui_tilemap_rendering.md`、`inspector_panel.md`
  - Game：暂未开篇，待产品路线明确后补充。
- **其他**
  - `open_questions.md`：开放议题与待决策项。

所有文档默认使用中文描述，必要时辅以英文名词；新名词首次出现需给出解释。

## 维护约定
- 文档遵循“结构分层、职责清晰”的原则：新增内容应归入现有层级或补充新专题。
- 对运行时代码的契约更新，应同步更新相关文档并在 `open_questions.md` 标记版本。
- CLI 文档为遗留方案，除故障排查外不再扩展；GUI 为主力前端，Game 相关设计暂缓。
- 变更完成后需更新 `CHANGELOG` 或提交说明，确保团队能够追踪架构演进。
