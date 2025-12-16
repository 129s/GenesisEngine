# GenesisEngine 路线图（Core + 工具链优先）

> 2025-12-16 更新：优先夯实 Core 模拟与回归/Soak/Worldline 工具链（确定性、可观测、可对比、可解释）；Sandbox GUI 作为可选调试前端暂缓推进，避免 UI 牵引机制演进。

## 背景与更新要点
- 交互式/渲染型 CLI（ASCII/实时渲染等）不再作为主入口；回归与批跑以 Headless 工具 `genesis-runtime-cli` 为主。
- Sandbox GUI 代码与基础能力已存在，但当前迭代不以 GUI 为主战场（避免 UI 工作量遮蔽机制与可回归验证）。
- 运行时架构调整为：单线程 Core Runtime 提供确定性模拟；Runtime Facade 暴露控制/查询/Telemetry 契约；前端通过只读快照消费数据，禁止直接操作 ECS。
- 文档体系同步：GUI 架构、世界模型、运行时 API 均已拆分到 `docs/architecture`，路线图聚焦阶段目标与风险。
- Runtime Facade 已提供事件命令队列与 `latestSnapshotDiff`，为 GUI Inspector 与自动化回放提供事件注入与断言基线。

## 目标与范围
- 提供稳定、可扩展的模拟核心，前端统一经 Runtime Facade 访问。
- 以 Headless 回归/Soak/Worldline 工具链为主战场，形成“可回归验证 + 可解释观测 + 可对比证据”的闭环。
- 支撑多需求、多系统互联的 Agent 行为，同时确保长时间运行的性能与诊断手段。
- GUI 作为可选调试前端：仅在需要交互式观测时启用，不绑定主线里程碑。

## 架构基线
- **Simulation Kernel**：`SimulationHost/SimulationContext/Scheduler` + 系统集合（Movement/Needs/Planner/ActionExecutor/Resource 等），单线程按 `SimulationClock` 推进；世界数据以 `WorldDatabase`（`world.json + map_{id}.json`）为输入输出契约。
- **Runtime Facade**：控制面（`run`/`pause`/`step`/`setSpeed`）、查询面（`latestSnapshot`、`latestSnapshotDiff`、WorldAtlas、TelemetryBuffer）、事件入口（命令队列/事件注入 + 快照对比，支撑回放与断言）。
- **Presentation 层**：
  - Headless（主力）：`genesis-runtime-cli` 负责回归/批跑/Soak/Worldline 产出与对比。
  - GUI（可选/暂缓）：GLFW + OpenGL + Dear ImGui，RuntimeBridge 后台线程 + 环形快照缓冲。
  - Game（探索中）：未来与 GUI 共享 Runtime 契约。
- **可观测性**：Telemetry Schema 与 Snapshot 双缓冲是协议演进核心；所有前端使用 schema version 校验以避免破坏性更新。

## 近期（P0，稳定与补齐基础）
- 运行时与并发安全
  - [x] 双缓冲 `SimulationSnapshot`（已引入 `SimulationSnapshotBuffer`，前端读取线程安全）
- [x] 快照比较与事件注入 API（便于 E2E 与重放，已上线 diff 结构与命令队列）
- GUI 调试体验
  - [x] Inspector 视图：实体列表/详情/地图联动的基础版本已上线，后续补充人格/Traits 等高级信息
  - [ ] RuntimeBridge Telemetry 配置：巩固指标采集与阈值告警面板草案
- 测试与回归
  - [ ] 端到端闭环用例：NeedSatisfier → ActionExecutor → Need 恢复
  - [ ] 24 小时离线长时模拟（指标追踪：饥饿/旅行成本/库存告警）
- 文档与规范
  - [x] 整理 `docs` 目录、合并路线图
  - [ ] 扩写运行时与快照协议说明（面向 GUI 接入）

## 阶段规划

### 近期（P0 · GUI 框架稳定与运行时加固）
> 阶段状态：执行中（自 2025-10-19）；每周日同步风险与燃尽图。
- **当周聚焦**
  - 快照差异与事件注入 API：已交付 `latestSnapshotDiff`、命令队列与事件日志；后续评审 GUI 接入点与脚本化示例。
  - GUI Inspector：基础视图交付（实体列表/详情/地图联动），剩余项：人格/Traits、Portal 详情、历史曲线。
  - GUI 烟雾巡检脚本：替换原 CLI smoke，圈定最小自动化覆盖与验收脚本。
- 运行时与协议
  - [ ] 双缓冲 `SimulationSnapshot` 与版本标记，确保 GUI 前端安全读取（CLI 停用但保持编译通过）。
  - [x] 快照差异与事件注入 API，支撑回放、断言与工具链。
  - [ ] 长时运行（24h）回归脚本，纳入指标追踪（饥饿/库存/旅行成本）。
- GUI 主线
  - [x] 里程碑 1-2：GLFW + ImGui Docking 框架、RuntimeBridge 后台线程、WorldAtlas 静态视图。
  - [ ] 里程碑 3（proposal）：OCEAN 人格/画像、命名标签、模拟时间语义化、第二资源点验证。
  - [ ] 里程碑 3 文档补齐（可视化策略/Scene View 说明）。
  - [x] 里程碑 4：Inspector 面板（实体列表、详情、Map 联动基础版完成；细化项见 `docs/architecture/interface/sandbox/inspector-panel.md`）。
- 工程与支持
  - [ ] GUI 烟雾测试流水线：巩固可执行脚本 + 关键断言（CLI 流程暂停）。
  - [ ] 构建流水线：Windows/Linux GUI 构建、符号与依赖打包。

### 短期（P1 · GUI 能力扩展与遥测体系）
- 调试体验
  - [ ] Inspector 深化：Needs/ActionQueue/位置轨迹、关注列表。
  - [ ] Telemetry 面板图表化：长期趋势、阈值告警、对比视图。
  - [ ] 地图视图优化：视锥裁剪、抽样/热力模式、名称/标签层管理。
- 世界生成
  - [ ] WorldGen 面板参数持久化（预设/导入导出）。
  - [ ] 布局热加载与 Undo/Redo，暴露验证报告（孤岛/资源不足）。
- 工具链
  - [ ] GUI 录制/截图工具、Headless 模式生成快照。
  - [ ] Telemetry 序列化格式固定（JSONL/Parquet 评估），配套分析脚本。

### 中期（P2 · 行为系统与世界互联）
- 多需求规划
  - [ ] 能量/社交/安全等 Utility 链路与 Planner 模块。
  - [ ] 冲突与调度：地点容量、排队、抢占策略。
- 信息流与社交
  - [ ] Fact/Observation/Rumor 管线，模型化信息传播失真。
  - [ ] 基础对话系统：社交需求恢复、信息交换、信任度影响。
- 世界事件
  - [ ] 天气/节律事件影响需求阈值与行动代价。
  - [ ] 任务/委托系统：可拆分目标链、失败恢复策略。
- 可观测性
  - [ ] 遥测指标分层（即时/区间/趋势）、异常检测、报告生成。

### 中远期（P3-P4 · 规模、性能与生态）
- 性能与规模
  - [ ] 性能基线采样：Top N 热点、内存画像、线程调度策略。
  - [ ] 批处理与缓存：路径/规划计算复用、热点降频。
  - [ ] 分块加载（Chunked Tile Graph）落地，支撑大地图按需更新。
- 生态扩展
  - [ ] 数据驱动配置与热加载（行为原型、数值、对话模板）。
  - [ ] 插件系统评估（Lua/JS），定义安全沙箱与扩展 API。
  - [ ] 跨平台打包与发布（Win/macOS/Linux），自动化交付流水线。
- 知识体系
  - [ ] 文档站/示例场景库、教程化引导。
  - [ ] GUI/Runtime API 版本策略与升级指南。

## CLI 暂停策略
- 暂停对外发布与支持，仅保留源码与最小编译验证，作为必要时的回退手段。
- 原有烟雾测试与自动化脚本并入 GUI 流程；CLI 流水线停用，不再执行回归。
- 文档保留归档版本并标注状态，如需等价操作全部转向 GUI 指南。

补充：Headless 回归工具 `genesis-runtime-cli` 属于 Runtime 自动化入口，不在“暂停”范围；其用法见 `docs/guides/headless_runtime_cli.md`。

## 里程碑速览
- P0：Runtime 快照双缓冲 + GUI 里程碑 4（Inspector）+ 24h 回归落地。
- P1：GUI 调试工具完善（Telemetry/WorldGen/录制）+ Schema 固定。
- P2：多需求规划 + 信息流/事件 + 遥测分层。
- P3：性能画像 + 批处理/分块 + 扩展生态。
- P4+：模块化扩展 + 插件/脚本化 + 全平台交付。

## 风险与依赖
- GUI 渲染性能：大图裁剪/批处理不足会导致帧率下滑；需持续 Profiling。
- 快照协议演进：需确保 GUI 快照消费与版本兼容；CLI 停用阶段仍需维持基础编译通过以防回退。
- 多线程交互：RuntimeBridge 与 UI 通信必须保持锁策略/队列边界，防止死锁与数据竞态。
- 长时运行稳定性：需尽快建立 24h 回归与内存泄漏检测。
- 依赖治理：GLFW/ImGui/OpenGL 驱动差异需在 CI 中覆盖多平台。

## 参考资料
- 架构索引：`docs/architecture/README.md`
- 运行时 API：`docs/architecture/foundation/runtime-api.md`
- GUI 设计：`docs/architecture/interface/sandbox/sandbox-gui.md`、`docs/architecture/interface/sandbox/sandbox-gui-sim-loop.md`
- 世界模型：`docs/architecture/world/world-model.md`、`docs/roadmap/MVP_SCENE_INTERACTIVE.md`
- 进度概览：`docs/status/progress-summary.md`

## Roadmap 与 Status 边界
- Roadmap：聚焦未来目标、主题与里程碑，描述优先级与能力边界。
- Status：记录当前进展、风险与下一步，链接到 Issues/PR 与本路线图条目。
- Backlog：以 Issue/Milestone 为准；`docs/status/todo.md` 用作临时收集，更新时需关联到路线图阶段。
