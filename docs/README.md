# GenesisEngine 文档总览

本文档目录汇总当前可用的技术文档、路线图与开发指引，便于快速定位信息与协作。

## 目录结构
- 架构（设计、原则、系统划分）：`docs/architecture/`（总览：`docs/architecture/README.md`）
- 路线图（权威版、里程碑与历史）：`docs/roadmap/`（总览：`docs/roadmap/README.md`）
- 指南（构建、运行、工具）：`docs/guides/`（CLI：`docs/guides/sandbox-cli.md`，已归档）
- 状态（进度、待办）：`docs/status/`（进度：`docs/status/progress-summary.md`，待办：`docs/status/todo.md`）
- 评审（UI/UX/技术评审）：`docs/status/reviews/`

- 故障排查（GUI/运行时常见问题）：`docs/troubleshooting/`（GUI：`docs/troubleshooting/sandbox_gui.md`）

- 文档组织与边界：`docs/handbook/DOCS_ORGANIZATION.md`
- 架构概览：`docs/architecture/README.md`
- 愿景与设计原则：`docs/architecture/VISION.md`
- 世界模型（Scene/Interactive）：`docs/architecture/WORLD_MODEL.md`
- 世界生成流程：`docs/architecture/WORLD_GENERATION.md`
- 行为与人格建模：`docs/architecture/AGENT_PERSONALITY_BIG5.md`
- Graph → ASCII 映射（CLI 遗留）：`docs/architecture/GRAPH_TO_GRID.md`
- Tilemap 融合方案：`docs/architecture/TILEMAP_INTEGRATION.md`
- 区块化 Tile 图探索：`docs/architecture/CHUNKED_TILE_GRAPH.md`
- 运行时与沙盒 CLI（归档）：`docs/guides/sandbox-cli.md`
- 路线图（权威版）：`docs/roadmap/README.md`
- MVP：Scene/Interactive 节点树重构：`docs/roadmap/MVP_SCENE_INTERACTIVE.md`
- MVP：噪声随机地图 + 固定刷新资源 + NPC 循环：`docs/roadmap/MVP_NOISE_MAP.md`
- 历史路线图：`docs/roadmap/history/`
- 项目进度概览：`docs/status/progress-summary.md`
- 临时待办（转向 Issue/里程碑前的过渡）：`docs/status/todo.md`

## 使用与构建
- 沙盒 CLI：支持暂停；如需参考历史材料，可见 `docs/guides/sandbox-cli.md` 与 `scripts/run_sandbox_cli.ps1`（不再维护）。
- 沙盒 GUI：参考 `docs/architecture/SANDBOX_GUI.md` 与 `docs/troubleshooting/sandbox_gui.md` 获取当前主流程说明。
- 运行时封装：核心模拟逻辑以 `genesis_runtime` 动态库形式对外提供 API，前端二进制（如 CLI/GUI/Game）复用同一接口。

## 文档约定
- 路线图以 `docs/roadmap/README.md` 为权威来源。历史分篇保留于 `docs/roadmap/history/`。
- 术语：实体组件（EnTT）、需求/行为（Needs/Actions）、事实/传闻（Fact/Rumor）、快照（SimulationSnapshot）。

## 下一步建议
- 将 `docs/status/todo.md` 中仍然有效的条目迁移到 Issue 与 Milestone，按 `docs/roadmap/README.md` 的阶段划分追踪。

