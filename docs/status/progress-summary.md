> 文档范围：面向当前/近期开发的进度与风险汇总；长期计划以 `docs/roadmap/README.md` 为准；具体任务请参考仓库 Issues/Milestones。
# 项目进度概览

## 最新进展（本次）
- 运行时快照 diff 与事件注入 API：
  - 新增 `Runtime::latestSnapshotDiff()` 与 `SimulationSnapshotDiff`，支持快速检测资源/需求/行动变化并携带事件日志。
  - 引入 `Runtime::enqueueEvent` 命令队列，在每个模拟步执行并写入 `RuntimeEventReport`，用于 GUI 交互与自动化回放。
  - 扩展双缓冲快照结构，捕获命令执行结果并在文档中更新使用指南。
- Sandbox GUI Inspector 基础版：
  - Inspector 面板新增实体列表（Agent/Resource/Node 分组）与搜索，详情面板支持查看需求、行动、Planner 结果与移动进度。
  - Map View 增加选中高亮、Agent 圆环强调及“一键定位/Scene 打开”按钮；支持跟随模式自动切换 Scene View。
  - 将 Runtime 事件日志与需求 diff 整合进 Inspector，便于定位命令执行结果与本帧变化。
- 构建与测试稳定性改进：
  - 为 `genesis_sandbox_cli` 增加 `Genesis::Engine` 链接，修复噪声世界生成符号缺失导致的链接错误。
  - 为 `genesis_runtime_tests` 增加 `Genesis::Engine` 链接，避免 Windows 下跨模块静态库初始化差异带来的崩溃。
  - 在 `Runtime` 中初始化 SPDLOG 缺省 logger（最佳努力，不干扰外部设置）。
  - 全部测试通过（33/33）。
- 世界加载与可达性：
  - 放宽 WorldLoader 对 `coord_global`/`anchors`/`local_coord` 的强制要求，兼容旧 JSON；存在时解析回填。
  - 修正初始化出生点策略：优先选择“可达的资源所在位置”，避免噪声图孤岛导致长期无法消费。
- 文档与路线图：
  - 新增 `docs/roadmap/MVP_SCENE_INTERACTIVE.md`（Scene/Interactive 节点树重构的 MVP 范围与验收标准）。
  - 在 `docs/roadmap/README.md` 补充引用。

## 已完成工作
- 核心框架：重建 C++20 构建骨架，集成 spdlog、entt、nlohmann::json、gtest，并实现基础的 Engine 循环、离散时间 SimulationClock 以及事件总线。
- 世界模型：完成 WorldRegistry、JSON 加载器与示例地图，提供资源生成点、路径边和快速查询接口。
- 资源系统：实现库存与产出调度，支持消耗事件、低库存告警，并新增单元测试保证补货逻辑正确。
- 需求与行为：实现 NeedSystem、NeedSatisfier，并通过 HungerPlanner 将饥饿需求与资源消耗串联，增加位置与拥挤度感知、最短路径选择及决策记录。
- 移动系统：新增 MovementSystem，通过 MovementIntent/MovementState 驱动 Agent 沿图搜索路径移动，同时调整 NeedSatisfier 仅在抵达目标后执行资源消耗，并补充对应单元测试。
- 任务执行：落地 ActionExecutor 及 ActionQueue，将 Planner 决策映射到移动/消耗任务，联动 MovementSystem、ResourceSystem 与 NeedSystem，补充对应单元测试验证任务调度与饥饿恢复。
- 遥测与日志：引入 TelemetryBuffer 捕获资源快照、需求状态、规划决策，并新增行动队列与代理位置快照；周期性输出资源/饥饿/平均旅行成本等核心指标。
- 实时沙盒：抽象出 genesis_runtime 动态库，并基于其实现 sandbox_cli，提供实时步进/暂停与 ASCII 观测；新增 scripts/run_sandbox_cli.ps1 脚本快速启动调试会话。
- 测试覆盖：补充 SimulationClock、WorldRegistry/Loader、ResourceSystem、NeedSatisfier、HungerPlanner 以及 TelemetryBuffer 的单元测试，维持自动化构建通过；扩展 ctest 增加 runtime/CLI 烟雾测试。
- 规划路线：整理 Utility Planner 迭代路线图，明确各阶段目标、风险与依赖。
- 工具链验证：在 MinGW 环境下确认 `gcc`/`g++` 15.2.0 与 `mingw32-make` 4.4.1 可用，为后续本地构建提供保障。

## 未完成与待推进事项
- 多需求规划：仅实现饥饿规划，能量/社交等需求的 Utility 评估与行为链尚未落地。
- 遥测分析：需要进一步构建可视化/统计工具，对遥测数据进行长期对比和异常报警。
- 系统互联：天气、事件、经济、社交网络等子系统尚未对接，目前为单一需求闭环。
- CI 与长时模拟：计划中的 24 小时回归和指标比对尚未配置，仍需在 pipelines 中补全。
- 性能优化：规划/遥测引入更多计算后，需要在后续阶段进行性能与内存分析。
- 端到端测试：补充完整 E2E 场景，用以验证 Planner → ActionExecutor → Needs 的闭环行为。
- 运行时封装：扩展 runtime API（快照对比、事件注入等），并实现 sandbox_gui、game 前端以复用统一模拟核心。
