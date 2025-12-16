> 文档范围：面向当前/近期开发的进度与风险汇总；长期计划以 `docs/roadmap/README.md` 为准；具体任务请参考仓库 Issues/Milestones。
# 项目进度概览

> 当前方向：优先推进 Core 模拟与回归/Soak/Worldline 工具链；GUI 相关工作暂缓（若已存在实现则作为可选调试前端保留）。

## 最新进展（本次）
- 回归与可观测性闭环：
  - Core Regression 继续作为门禁；Soak/Worldline 作为软对比证据（不门禁）。
  - Worldline Chronicle v0：按窗口输出宏观证据，支撑跨 seed/配置/版本对比与母题抽取（见 `docs/architecture/foundation/worldline-chronicle.md`）。
- 资源经济与生产链（机制侧）：
  - 工坊生产：引入生产作业时间、并行槽位与临界需求抢占中断（并补齐协议文档）。
- 文档整顿：
  - 以代码已落地内容固化 Spec；设想/规划集中到 Proposal；并补齐对齐矩阵（Agents 层已完成一轮）。
- GUI（暂缓）：
  - 历史集中迭代记录已归档：`docs/status/history/2025-12-15_gui-runtime-ux.md`。

## 已完成工作
- 核心框架：重建 C++20 构建骨架，集成 spdlog、entt、nlohmann::json、gtest，并实现基础的 Engine 循环、离散时间 SimulationClock 以及事件总线。
- 世界模型：完成 `WorldDatabase`、加载/保存与示例世界（`world.json + map_{id}.json`），提供 Map/Scene/Interaction/Portal 查询与 Map 图最短路。
- 资源系统：实现库存与产出调度，支持消耗事件、低库存告警，并新增单元测试保证补货逻辑正确。
- 需求与行为：实现 NeedSystem、NeedSatisfier（固定规则打分选点），并通过 ActionExecutor/ResourceSystem 串联需求恢复与资源消耗；Telemetry 输出 `plannerDecisions[]` 等用于解释“为何选它”。
- 移动系统：新增 Movement2DSystem，运行时采用 Map 内直线移动语义；Need/Action 在抵达目标后执行资源消耗，并补充对应单元测试。
- 任务执行：落地 ActionExecutor 及 ActionQueue，将 Planner 决策映射到移动/消耗任务，联动 MovementSystem、ResourceSystem 与 NeedSystem，补充对应单元测试验证任务调度与饥饿恢复。
- 遥测与日志：引入 TelemetryBuffer 捕获资源快照、需求状态、规划决策，并新增行动队列与代理位置快照；周期性输出资源/饥饿/平均旅行成本等核心指标。
- 实时沙盒：抽象出 genesis_runtime 动态库，并基于其实现 sandbox_gui，提供实时步进/暂停与可视化调试；参考 `docs/guides/sandbox_gui_smoke.md` 获取操作指引。
- 测试覆盖：补充 SimulationClock、WorldDatabaseLoader、ResourceSystem、NeedSatisfier、Planner 以及 TelemetryBuffer 的单元测试，维持自动化构建通过；扩展 runtime smoke/regression 测试。
- 规划路线：整理 Utility Planner 迭代路线图，明确各阶段目标、风险与依赖。
- 工具链验证：在 MinGW 环境下确认 `gcc`/`g++` 15.2.0 与 `mingw32-make` 4.4.1 可用，为后续本地构建提供保障。

## 未完成与待推进事项
- 多需求规划：仅实现饥饿规划，能量/社交等需求的 Utility 评估与行为链尚未落地。
- 遥测分析：需要进一步构建可视化/统计工具，对遥测数据进行长期对比和异常报警。
- 系统互联：天气、事件、经济、社交网络等子系统尚未对接，目前为单一需求闭环。
- CI 与长时模拟：计划中的 24 小时回归和指标比对尚未配置，仍需在 pipelines 中补全。
- 性能优化：规划/遥测引入更多计算后，需要在后续阶段进行性能与内存分析。
- 端到端测试：补充完整 E2E 场景，用以验证 NeedSatisfier → ActionExecutor → Needs 的闭环行为。
- 运行时封装：扩展 runtime API（快照对比、事件注入等），并实现 sandbox_gui、game 前端以复用统一模拟核心。

## 下一阶段聚焦（建议 · 2 周）

> 目标：把“可回归 + 可解释 + 可对比”的证据链做厚，支撑你要的世界线/母题分析框架迭代。

- Worldline 证据增强：补充更贴叙事的宏观窗口特征（阶段切分/变点提示、主体行为分布的漂移摘要）。
- 母题工具链 v0：让 `worldline → 候选母题` 的离线分析脚本可批跑、可回放定位窗口/实体（以“尽量无预设”作为约束）。
- Soak 基准集：固定一组配置/seed 作为对照集合，建立版本间的软对比报告模板（不门禁，但强留档）。
- 文档对齐扩面：把 Interface/Meta 中仍夹杂的“设想/承诺”迁移到 Proposal，并在原文保留指针。
