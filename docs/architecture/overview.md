# GenesisEngine · Architecture Overview

GenesisEngine 的使命是构建“可扩展、可观测、可复用”的涌现式叙事模拟核心。我们围绕一个单线程推进的 Runtime（Engine）展开，向外暴露稳定的 API，向上支持多种前端形态。本文概览核心分层、运行循环与并发边界。

## 架构分层（自下而上）
- **Core Runtime**
  - `WorldRegistry`：管理分层节点图、Portal、锚点与交互节点，提供拓扑/资源查询。
  - 系统集合：Movement、Attributes/Needs、Planner/Actions、Resources 等纯逻辑系统，按离散 `SimulationClock` 推进。
  - Telemetry 缓冲：模拟线程只写，前端只读；保证数据快照的一致性与线程安全。
- **Runtime 封装**
  - 控制面：暂停/恢复、步进、调速、生成世界等命令队列化执行。
  - 查询面：提供 `TickTelemetry` 与 `WorldAtlas` 的只读视图，附带版本号。
  - 并发边界：前端不直接访问 ECS/Registry，只能通过 Telemetry/Atlas 读取数据。
- **前端适配**
  - CLI（遗留）：ASCII 布局渲染，主要用于最初阶段的调试。
  - GUI（主力）：GLFW + ImGui，实现 Map/Scene 视图、Inspector、Telemetry 面板。
  - Game（规划中）：面向玩家的最终客户端，共享 runtime API。

## 世界表达（双轨抽象）
- **分层节点图（Layered Node Graph）**  
  - 逻辑层次：`Region → District → Area → Anchor/Interaction`。  
  - 节点记录 `mapId`、局部坐标、角色（锚点、交互点、Portal 等），边描述连通性与移动成本。  
  - 提供宏观寻路、资源定位、运行时逻辑驱动的基础。
- **Tilemap（表/里视图）**  
  - 每张 map 拥有 `outsideView`（挂载到父 map 的呈现方式）与 `insideView`（当前视图的完整瓦片层）。  
  - Anchor/Interaction/Portal 在 Tilemap 的对象层中标记，以支持渲染定位与局部寻路。  
  - MapConfig 递归生成：从 root map 出发，按模板生成子 map，形成完整的空间树。

两条轨道通过 Portal/锚点契约对齐：宏观逻辑在节点图上行走，GUI 在 Tilemap 中渲染细节；两者共享 `mapId + 坐标`，确保模拟与展示同步。

## 运行循环
1. **感知（Perception）**：收集世界事件，更新属性/需求缓存。
2. **需求评估（Needs Evaluation）**：依据属性、人格（OCEAN）与 Trait 计算需求强度。
3. **决策（Decision）**：Planner 将需求转化为目标/动机，选定目标锚点或交互节点。
4. **行动（Action Execution）**：Movement/Interaction 系统执行规划好的行动序列，驱动世界状态变化。
5. **记录（Bookkeeping）**：Telemetry 快照、日志、统计指标、诊断信息。

每个阶段都受 `SimulationClock` 控制，默认离散时间步可配置。逻辑寻路与局部寻路分层执行：  
- 纯模拟或无 GUI 时仅执行逻辑寻路（跨 map 的 Portal 链 + 交互节点）。  
- GUI 可见时补充局部 Tilemap 寻路，用于渲染 NPC 在视口内的真实轨迹，与逻辑进度保持近似同步。

## 并发与数据流
- 模拟线程：唯一修改世界状态的线程；所有命令必须排队在此执行。
- 前端线程：读取 `TickTelemetry` 与 `WorldAtlas` 的最新版本，不跨线程访问 ECS。
- 版本机制：世界变更提升 `world_version`，前端据此刷新 Atlas/Tilemap 缓存；Telemetry 包含 `schema_version` 保障协议演进。

## 参考文档
- 运行时协议详见 `runtime_api.md`
- 世界模型与生成详见 `world_model.md`、`world_generation.md`
- 前端渲染与调试详见 GUI 系列文档
