# Tilemap 与 节点图融合方案（可行性与落地路径）

## 结论（TL;DR）
- 可行：将“节点图（LocationGraph）”作为语义/拓扑层的元结构，在展示与细粒度导航层使用“Tilemap（栅格地图）”，是合理且常见的分层建模方式。
- 推荐：采用分层导航（Hierarchical Pathfinding）：图层负责跨地点/房间跳转，Tile 层负责室内/局部路径与碰撞。
- 迁移策略：保持现有图与系统不变，最小增量地引入 Tile 资源、入口/门坐标（Portal）、实体局部坐标与占据网格。

## 架构思路
- 元结构（语义/拓扑层）：保留现有 `LocationGraph{ nodes, edges, spawns }` 作为“哪些地点相连、可走、代价多少”的高层抽象。
- 呈现/细粒度（Tile 层）：为每个地点（通常到 Room/Building 粒度）提供一张或多张 Tile 层（地面、墙体、装饰、障碍）及门/入口位置。
- 跨层绑定（关键）：
  - `LocationNode.id` ↔ Tile 子图/区域（一个或若干 Tile 区域）。
  - `PathEdge` ↔ 门/入口（Portal）：为边的两端提供对应的 Tile 坐标（或一段门槛线段）。

## 数据模型（建议增量）
- 新增：`Tilemap` 资源（独立文件，如 `data/tilemaps/<location_id>.tmj` 或打包 atlas），描述：
  - 尺寸：`width,height`；
  - 层：`ground/walls/props/colliders`；
  - 障碍栅格：二值或权重（用于 A*）。
- 新增：`LocationLayout`（可并入世界 JSON 或独立文件）
  - `location_id` → `{ tilemap_path, bounds_in_world(optional), portals[] }`
  - `portals[]`：`{ to: <LocationId>, from_tile:(x,y), to_tile:(x,y) }`（或一段范围）。
- 新增：实体局部坐标组件
  - `AgentTransform{ location: LocationId, tileX:int, tileY:int }`
  - 现有 `AgentLocation.location` 可保留，但建议合并/替换为带坐标的变体。
- Telemetry 扩展
  - `AgentSnapshot` 增加 `tileX,tileY`（可选）；
  - 便于 GUI/录制精细可视化。

## 导航与执行
- 分层寻路（推荐 HPA* 思路）：
  1. 高层：在 `LocationGraph` 上做 `A*`/Dijkstra，路径为节点序列 `[L1 → L2 → ...]`。
  2. 细层：在每段 `(Li → Li+1)` 中，使用 Tile 障碍层做局部 `A*`（起点为上一个段的到达 Portal 坐标）。
  3. 门/入口（Portal）作为跨地点的锚点；在同地点内，使用 Tile 网格直接寻路。
- 行为系统对接：
  - `MoveTo(location)` → 细化为 `MoveToTile(tileX,tileY)` 队列，或引入 `PathFollowing` 组件。
  - `ConsumeResource` 等行动不变，仅把行动发生点绑定到对应 Tile 区域（通常为资源点附近若干 Tile）。

## 生成与资产
- 手工布局（P0）：为 Demo/小地图直接提供每个地点的 Tilemap 与 Portal 坐标（最快验证路径）。
- 半自动（P1）：基于“地点原型（Tavern/Residence）”拼装 Tile 片段，自动打孔出口，按 `PathEdge` 生成 Portal。
- 全自动（P2+）：由世界生成器同时产出 `LocationGraph`、`Tilemap`、`Portal` 与布局 JSON（保持 `id` 一致）。

## 与现有代码的契合点
- 不破坏：`LocationGraph`、`WorldRegistry`、`ResourceSystem`、`ActionSystem` 可原样保留。
- 需要扩展：
  - 新增 `AgentTransform`（tile 级位置）；
  - MovementSystem 支持“图层→Tile 层”分解与门口对齐；
  - Telemetry 可选增加 Tile 坐标；
  - GUI/可视化加载 Tilemap 并渲染；CLI 可继续使用抽象布局或增加粗粒度 Tile 预览。

## 兼容策略（最小落地路径）
1. 资产：为现有 3 个房间/2 个建筑提供简易 Tilemap（10×10 之类）与 Portal 坐标。
2. 组件：引入 `AgentTransform`（默认把现有代理放在其 `LocationId` 的一个合法 Tile 上）。
3. 寻路：同地点内用 Tile `A*`，跨地点沿 `PathEdge` 跳 Portal。
4. 行为：`MoveTo(LocationId)` → 选该地点的“落脚 Tile”（比如中心/入口近邻），逐步细化到 `MoveToTile`。
5. Telemetry：保持兼容；GUI 若存在则读 Tile 数据；CLI 继续使用矩阵布局。

## 风险与缓解
- 一致性：`PathEdge` 必须有对应可用的 Portal；
  - 缓解：构建时校验“边端点的 Tile 可达/不越界/非障碍”。
- 性能：大地图 Tile 层内存与寻路代价上升；
  - 缓解：分区/分块（chunk）、只加载可见/邻接地点的 Tile；门口周围留缓存。
- 工具链：Tile 资产制作成本；
  - 缓解：采用 Tiled(.tmx/.tmj) 或自研极简 JSON；提供模板拼装。

## 结语
- 该方案与当前“图→快照→前端渲染”的解耦理念一致：图层负责语义与全局连通，Tile 层负责视觉与微观移动。
- 推荐从 P0（手工 Tile + Portal）开始验证，逐步演进到生成管线集成。