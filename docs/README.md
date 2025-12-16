# GenesisEngine 文档总览

本文档目录汇总当前可用的技术文档、路线图与开发指引，便于快速定位信息与协作。

## 目录结构
- 架构（设计、原则、系统划分）：`docs/architecture/`（总览：`docs/architecture/README.md`）
- 路线图（权威版、里程碑与历史）：`docs/roadmap/`（总览：`docs/roadmap/README.md`）
- 指南（构建、运行、工具）：`docs/guides/`（构建流程：`docs/guides/build_pipeline.md`，沙盒 GUI 烟雾测试：`docs/guides/sandbox_gui_smoke.md`）
- 状态（进度、待办）：`docs/status/`（进度：`docs/status/progress-summary.md`，待办：`docs/status/todo.md`）
- 评审（UI/UX/技术评审）：`docs/status/reviews/`

- 故障排查（GUI/运行时常见问题）：`docs/troubleshooting/`（GUI：`docs/troubleshooting/sandbox_gui.md`）

- 文档组织与边界：`docs/handbook/DOCS_ORGANIZATION.md`
- 内核回归闭环（Headless 优先）：`docs/handbook/CORE_REGRESSION.md`
- 架构概览：`docs/architecture/README.md`
- 愿景与设计原则：`docs/architecture/meta/vision.md`
- Telemetry Schema（TickTelemetry）：`docs/architecture/foundation/telemetry-schema.md`
- 世界模型：`docs/architecture/world/world-model.md`
- 世界生成流程：`docs/architecture/world/world-generation.md`
- Agent 模式规范（已落地）：`docs/architecture/agents/agent-model-v0.md`
- Agent 文档-代码对齐矩阵：`docs/architecture/agents/agent-implementation-status.md`
- Proposal：Big5/Traits/数据驱动 Needs：`docs/architecture/proposals/agents/agent-personality-big5.md`
- Graph → ASCII 映射（历史资料）：`docs/architecture/archive/graph-to-grid.md`
- 渲染表示：`docs/architecture/world/world-representation.md`
- 区块化 Tile 图探索：`docs/architecture/world/chunked-tile-graph.md`
- 路线图（权威版）：`docs/roadmap/README.md`
- MVP：Scene/Interactive 节点树重构：`docs/roadmap/MVP_SCENE_INTERACTIVE.md`
- MVP：噪声随机地图 + 固定刷新资源 + NPC 循环：`docs/roadmap/MVP_NOISE_MAP.md`
- 历史路线图：`docs/roadmap/history/`
- 项目进度概览：`docs/status/progress-summary.md`
- 临时待办（转向 Issue/里程碑前的过渡）：`docs/status/todo.md`

## 使用与构建
- 沙盒 GUI（可选调试前端）：参考 `docs/architecture/interface/sandbox/sandbox-gui.md` 与 `docs/troubleshooting/sandbox_gui.md` 获取主流程说明。
- 运行时封装：核心模拟逻辑以 `genesis_runtime` 动态库形式对外提供 API，前端二进制（如 GUI/Game）复用同一接口。
- Headless 工具：`docs/guides/headless_runtime_cli.md`（用于脚本回放与回归）

## 文档约定
- 路线图以 `docs/roadmap/README.md` 为权威来源。历史分篇保留于 `docs/roadmap/history/`。
- 术语：实体组件（EnTT）、需求/行为（Needs/Actions）、事实/传闻（Fact/Rumor）、快照（SimulationSnapshot）。

## 下一步建议
- 将 `docs/status/todo.md` 中仍然有效的条目迁移到 Issue 与 Milestone，按 `docs/roadmap/README.md` 的阶段划分追踪。

