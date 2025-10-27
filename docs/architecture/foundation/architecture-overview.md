# GenesisEngine · Architecture Overview（v2）

GenesisEngine 的使命是构建“可扩展、可观测、可复用”的涌现式叙事模拟核心。v2 版本以单线程 `Engine` 为核心，采用“Map 内直线移动 + Map 图（有向）跨图拼接”的世界语义，对外暴露稳定的 Runtime API（快照/事件/命令），前端（Game/GUI）共享同一契约。本文概览分层结构、世界表达、并发边界与相关文档。

## 分层（自下而上）
- **Core Runtime（Engine + Systems）**
  - `WorldDatabase`（只读数据总线）：提供 `maps`、`mapEdges`、`scenes`、`interactions`、`portals`、`findInteraction(id)` 查询；运行时不暴露 ECS。
  - 系统集合（模块化）：
    - `Movement2DSystem`：Map 内直线移动；跨图由上层根据 `mapEdges` 决策并下发命令。
    - `ResourceSystem2D`：基于 `Interaction(kind=Resource)` 的库存/再生（capacity/regen）。
    - `Scheduler`：以固定顺序调度各系统。
  - `TelemetryCollector`：采集每步 `TickTelemetry`（agents、resources 等）。
- **Runtime 封装**
  - 控制面：`setPaused`、`requestStep`、`setSpeedMultiplier`、`start/stop`。
  - 查询面：`latestSnapshot()`、`latestSnapshotDiff()`；`worldDatabase()` 只读句柄（GUI 用于构建 Atlas）。
  - 命令面：`enqueueEvent(RuntimeEvent)` 串行执行命令（`world.db.load/save/reload`、`agent.create|move|stop|teleport|delete 2d`、`resource.consume` 等）。
  - 并发：模拟线程唯一写入者；前端仅读取快照/Atlas；双缓冲与版本号保障并发安全。
- **前端适配**
  - GUI（主力）：GLFW + ImGui；消费 Atlas/Telemetry，提供“世界（v2）/实体（v2）”操作面板。
  - Game：与 GUI 共享 Runtime 契约，作为最终用户入口（替代 CLI）。

## 世界表达（v2）：Map 图 + Scene 分组 + Interaction/Portal
- 世界层：世界由若干 Map 组成，Map 之间通过有向边（MapEdge）表征可达性与代价；跨图移动在此层拼接。
- 地图层：Scene 仅用于分组/布局与坐标继承；Interaction 是交互锚点（含 Portal/Resource/...），导航目标均指向交互点坐标。Map 内移动采用直线语义。
- 渲染层：Tilemap 仅用于表现，与运行时解耦；Atlas 聚合 maps/mapEdges 与每图的 scenes/interactions/portals。

说明：v2 暂未接入 Needs/Planner/ActionExecutor 等高级系统，后续将以模块形式回归并挂接到 `Scheduler`。

## 并发与数据流
- 模拟线程：唯一可以修改世界状态的线程；命令需排队执行。
- 前端线程：只读取 `SimulationSnapshot` / WorldAtlas，禁止直接访问 ECS。GUI 不做碰撞/局部寻路，只绘制 Atlas/Telemetry。
- 版本机制：世界数据变化会更新 `world_version`，前端可按版本刷新缓存；Telemetry 携带 `schema_version` 以便协议演进。

## 相关文档
- 运行时接口：[foundation/runtime-api.md](runtime-api.md)
- 世界模型：[world/world-model.md](../world/world-model.md)
- 生成（暂未接入 v2，历史文档供参考）：[world/world-generation.md](../world/world-generation.md)
- 行为与人格（历史/规划）：[agents/agent-personality-big5.md](../agents/agent-personality-big5.md)
- 渲染/调试：见 [interface/sandbox/sandbox-gui.md](../interface/sandbox/sandbox-gui.md)
