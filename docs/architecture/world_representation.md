# 世界表示 · Graph ↔ Tilemap

运行时通过“分层节点图（LocationGraph）+ Tilemap”双轨表达世界。Graph 提供语义与拓扑，Tilemap 提供几何与渲染，两者通过 Portal/Anchor 协作。本文说明二者职责分工、数据流与前端对接。

## 分层节点图职责
- **结构**
  - 节点层级：`Region → District → Area → Anchor/Interaction/Portal`。
  - 每个节点持有 `mapId`（可为空）、`coord_local`、`role` 等字段。
  - 边描述通行关系与代价，Portal 节点额外记录目标 map/坐标。
- **能力**
  - 宏观寻路：在节点图上执行 A* / Dijkstra，得到 Portal 链 + 目标交互节点。
  - 资源选址：通过节点角色/交互类型过滤，结合容量、密度等属性。
  - 世界查询：`WorldRegistry` 提供 children/parent/portal 查询，供系统与前端使用。
- **输出**
  - `WorldAtlas.nodes` / `edges` / `portals`，用于前端绘制与调试。
  - 供运行时系统（Resource、Planner、Movement）读取。

## Tilemap 职责
- **结构**
  - 每张 map 包含 `outsideView`（表面概览）与 `insideView`（细节场景）。
  - Tile 层描述地板、墙体、装饰；对象层标记 Anchor、Interaction、Portal。
  - `children[]` 指向子 map，实现表里切换。
- **能力**
  - 局部寻路：通过 Tile 碰撞/通行掩码，计算细粒度路径。
  - 渲染：GUI/前端根据 Tile 层绘制场景，Anchor/Portal 决定落点与高亮。
  - 交互定位：通过对象层精确定位资源点、事件触发器等。

## 协同机制
- **Portal = 特殊交互节点**
  - 在 Graph 中 Portal 属于 `NodeRole=portal` 的节点；在 Tilemap 中对应对象层标记。
  - Portal 节点携带 `fromMap/fromCoord` 与 `toMap/toCoord`，提供跨图路径与渲染门洞。
- **锚点同步**
  - Anchor 节点在 Graph 中保存 `mapId + coord_local`。
  - Tilemap 加载时验证锚点落在可通行 Tile；GUI 根据该坐标绘制 NPC。
- **世界版本**
  - 当 Graph 或 Tilemap 变更时，Runtime 提升 `world_version` 并下发新 Atlas。
  - 前端收到新版本需刷新 Tilemap 缓存，避免使用旧资产。

## 运行时数据流
1. **加载阶段**  
   - WorldLoader 解析 Graph；TilemapLoader 读取 inside/outside 视图与对象层。
   - WorldRegistry 建立节点/边索引；TilemapRegistry 保存 map 元数据。
2. **模拟阶段**  
   - Movement Planner 使用 Graph 规划 Portal 链；若需要局部路径则调用 Tilemap Pathfinder。
   - Resource/Interaction 系统依据 Graph 节点触发事件、更新库存。
3. **Telemetry 输出**  
   - `TickTelemetry`：为每个代理输出 `nodeId`、`mapId`、`localPosition`、`movementProgress`。
   - `WorldAtlas`：提供节点、边、Portal、Tilemap 元信息。
4. **前端消费**  
   - GUI 读取 Atlas 决定当前视图/子 map，使用 Telemetry 定位代理与资源。
   - CLI（遗留）使用 Graph 数据 → 布局映射 → ASCII 网格。

## 逻辑寻路与可视化寻路
- **逻辑寻路**：仅依赖 Graph，决定“下一步 Portal / 目标交互点”，适用于无 GUI 或视口外 NPC。
- **可视化寻路**：在 GUI 可见范围内，从当前 Anchor/Portal 到目标坐标执行 Tile 级寻路，并与逻辑进度同步。
- 逻辑结果拥有权威性；可视化层如果与逻辑进度偏离，需要快速 snap 回逻辑位置。

## 与其他模块的关系
- 生成流程：`world_generation.md` 负责输出符合本契约的 Graph + Tilemap。
- GUI 渲染：`sandbox_gui_tilemap_rendering.md` 依赖 Atlas/Telemetry 字段实现 Map/Scene 视图。
- Chunk/噪声扩展：`chunked_tile_graph.md` 在此契约基础上扩展大世界的提取与拼接。
