# 世界表示 · Graph ↔ Tilemap

> TL;DR：以分层节点图（LocationGraph）承载语义/拓扑，以 Tilemap 承载几何/渲染，通过 Portal 与 Anchor 将两条轨道绑定。推荐分层寻路（图层负责跨场景、Tile 层负责局部路径），并以最小增量引入 Tile 资源、Portal 坐标与实体局部坐标。

## 架构总览
- 分层节点图与 Tilemap 是解耦的两套数据：前者描述“哪里、如何连”，后者描述“长什么样、具体怎么走”。
- 双轨共享 ID、Portal、锚点坐标；Runtime 通过 `WorldAtlas` 向前端广播一致的快照。
- 迁移策略遵循渐进式：在不破坏现有 `LocationGraph` 的前提下，引入 Tile 级资源与组件。

## 分层节点图职责
- **结构**
  - 节点层级：`Region → District → Area → Anchor/Interaction/Portal`。
  - 节点字段包含 `mapId`（可空）、`coord_local`、`role` 等；边描述通行关系与代价。
  - Portal 节点额外记录目标 map/坐标，是跨地点通路的权威记录。
- **能力**
  - 宏观寻路：在图上执行 A*/Dijkstra，得到 Portal 链 + 目标交互节点。
  - 资源选址：按节点类型、容量、密度等过滤，服务资源与玩法系统。
  - 世界查询：`WorldRegistry` 暴露 children/parent/portal/交互节点查询，供系统与前端使用。
- **输出**
  - `WorldAtlas.nodes` / `edges` / `portals`，用于前端绘制与调试。
  - 运行时系统（Resource、Planner、Movement）直接读取图数据。

## Tilemap 职责
- **结构**
  - 每张 map 至少包含 `outsideView` 与 `insideView`，支持多层地面/墙体/装饰。
  - 对象层记录 Anchor、Interaction、Portal、碰撞体等元数据。
  - 可选 `children[]` 指向子 map，实现表里切换或分层显示。
- **能力**
  - 局部寻路：利用 Tile 碰撞/通行掩码求解细粒度路径。
  - 渲染：GUI/工具按照 Tile 层绘制场景，Portal/Anchor 决定落点与高亮。
  - 交互定位：对象层提供资源点、事件触发器的精确坐标。

## 协同机制
- **Portal 作为特殊交互节点**
  - Graph 中 Portal 属于 `NodeRole=portal`；Tilemap 中 Portal 对象提供 `from/to` 坐标。
  - portal 数据在加载阶段校验（目标 Map 存在、坐标可通行），并写入 `WorldAtlas.portals`。
- **锚点同步**
  - Anchor 节点保存 `mapId + coord_local`；TilemapLoader 验证锚点落在可通行 Tile。
  - GUI/录制按该坐标绘制 NPC 或资源，必要时提供 Snap 机制保证与逻辑一致。
- **世界版本广播**
  - Graph 或 Tilemap 任何变更都会提升 `world_version`；Runtime 下发新的 Atlas/快照。
  - 前端收到新版本后刷新渲染缓存与寻路索引，避免引用旧资产。

## 数据模型增量（建议）
- 引入独立 `Tilemap` 资产（`data/tilemaps/<map_id>.tmj|tmb`），描述尺寸、图层、障碍栅格。
- 引入 `LocationLayout` 或在世界 JSON 中扩展字段：
  ```json
  {
    "location_id": "TavernMain",
    "tilemap": "data/tilemaps/tavern_main.tmj",
    "bounds": [minX, minY, maxX, maxY],
    "portals": [
      { "to": "TavernBasement", "from_tile": [12,5], "to_tile": [3,8], "width": 2 }
    ]
  }
  ```
- 新增实体局部坐标组件 `AgentTransform{ location: LocationId, tileX: int, tileY: int }`。
- Telemetry 可选补充 `tileX/tileY`，便于 GUI、回放与分析。

## 导航与执行：分层寻路
1. **高层规划**：在 `LocationGraph` 上执行 A*/Dijkstra，得到节点序列 `[L1 → L2 → …]`。
2. **局部细化**：对每个 `(Li → Li+1)` 段，依据 Portal 坐标在 Tile 层执行 A*，生成局部路径。
3. **行为对接**：`MoveTo(location)` 细化为 `MoveToTile(tileX,tileY)` 序列或 `PathFollowing` 组件；Portal 切换时保持 Tile 坐标无缝衔接。
4. **权威性**：逻辑层路径永远权威，渲染/可视化可做插值但需在偏离时 Snap 回逻辑位置。

## 运行时数据流
1. **加载阶段**
   - WorldLoader 解析 Graph；TilemapLoader 读取 Tile 资产与对象层。
   - WorldRegistry 建立节点/边索引；TilemapRegistry 保存 map 元数据与障碍缓存。
2. **模拟阶段**
   - Movement Planner 使用 Graph 规划 Portal 链；进入可视范围时调用 Tile Pathfinder 细化。
   - Resource/Interaction 系统依据节点/Tile 定位，更新库存与事件。
3. **Telemetry 输出**
   - `TickTelemetry`：输出 `nodeId`、`mapId`、`localPosition`、`movementProgress`，可选附带 Tile 坐标。
   - `WorldAtlas`：广播节点、边、Portal、Tilemap 元信息。
4. **前端消费**
   - GUI 读取 Atlas 决定当前视图/子 map，结合 Telemetry 定位代理与资源。
   - 历史 CLI（ASCII）可继续仅消费 Graph + 布局映射，见 `../archive/graph-to-grid.md`。

## 迁移与兼容策略
1. **资产**：先为 Demo 世界提供手工 Tilemap 与 Portal 坐标（10×10 等小尺寸）。
2. **组件**：引入 `AgentTransform`，默认将代理落在其地点合法 Tile 上。
3. **寻路**：本地路径用 Tile A*，跨地点沿 `PathEdge` 跳 Portal；保留旧逻辑作为回退。
4. **行为**：`MoveTo(LocationId)` 先定位“落脚 Tile”（中心或入口附近），逐步过渡到完整 Tile 路径。
5. **Telemetry**：保持旧字段兼容，前端按需消费新增的 Tile 坐标。

## 风险与缓解
- **一致性**：Portal 端点必须可通行。构建/加载时校验 Tile 坐标不越界且非障碍。
- **性能**：大地图易造成内存与寻路压力。采用分块加载、LRU Cache、视区裁剪。
- **工具链成本**：Tile 资产制作门槛高。建议使用 Tiled（TMX/TMJ）或轻量 JSON，并提供模板/拼装脚本。

## 相关文档
- 生成流程：`./world-generation.md` 定义资产产出与校验。
- Chunk/噪声扩展：`./chunked-tile-graph.md` 探索大地图分块与拼接。
- GUI 渲染：`../interface/sandbox/sandbox-gui-tilemap-rendering.md` 说明前端如何消费 Atlas/Telemetry。
