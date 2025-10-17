# 世界模型（当前实现）

本节概述当前世界由哪些“组成部分”构成，以及它们在代码与数据中的位置。

## 静态世界数据（LocationGraph）
- 位置节点（LocationNode）：层级化地点（区域/建筑/房间/点），是否可导航
  - 定义：`include/genesis/world/WorldTypes.hpp:32`
  - 字段：`id`、`parent`、`name`、`kind`（`LocationKind`：Region/Building/Room/Point，见 `:25`）、`navigable`
  - 可选几何：`worldPos{x,y}`（建议，见下文“几何坐标与桥接”）
- 路径边（PathEdge）：地点之间的连通关系
  - 定义：`include/genesis/world/WorldTypes.hpp:40`
  - 字段：`from`、`to`、`cost`、`bidirectional`
  - 可选几何：`polyline[[x,y]...]`（建议，用于 Map View 绘制与更精确长度估计）
- 资源点（ResourceSpawn）：在某地点生成/供应资源
  - 定义：`include/genesis/world/WorldTypes.hpp:53`
  - 字段：`name`、`type`（`ResourceType`：Food/Drink/Social，见 `:47`）、`location`、`capacity`、`ratePerStep`
- 世界图容器（LocationGraph）：将以上三者组合成一个图
  - 定义：`include/genesis/world/WorldTypes.hpp:61`

JSON 载入格式（示例见 `data/world/demo_world.json`）与上述一致：
- `locations[]`（节点，含 `id/parent/name/kind/navigable`）
- `edges[]`（边，含 `from/to/cost/bidirectional`）
- `spawns[]`（资源点，含 `name/type/location/capacity/rate_per_step`）

扩展字段（建议，向后兼容，可选）：
- `locations[i].pos: { x: float, y: float }`（节点世界坐标，Map View 直接使用；缺省时由布局算法填充）
- `edges[i].polyline: [[x,y], ...]`（边的世界几何；缺省时用直线段 `from.pos → to.pos`）

加载流程：
- Loader 接口：`include/genesis/world/WorldLoader.hpp:16`、`:18`
- 载入到注册表：`WorldRegistry.setGraph(...)`（`include/genesis/world/WorldRegistry.hpp:17`）

## 运行时组织（WorldRegistry + ECS）
- WorldRegistry 负责组织、查询静态世界：
  - 查询：`findLocation`（`:23`）、`childrenOf`（`:24`）、`edgesFrom`（`:25`）
  - 统计：`locationCount`（`:29`）、`resourceSpawnCount`（`:30`）
  - 资源查询：`spawnsAt`（`:32`）、`allSpawns`（`:33`）
- 资源运行时组件（ECS）：
  - `genesis::world::components::ResourceSpawn`（定义：`include/genesis/world/components/ResourceSpawn.hpp:9`）
  - `genesis::world::components::ResourceInventory`（定义：`include/genesis/world/components/ResourceInventory.hpp:7`）
  - 资源系统负责产消与计数：`include/genesis/world/system/ResourceSystem.hpp`

> 注：静态图（拓扑与资源点定义）与运行时状态（库存数值、实体位置等）是分离的。静态部分经 `WorldRegistry` 管理，动态部分通过 ECS 组件与系统维护。

## 几何坐标与桥接（建议）
- 目的：统一 Map View（世界图）与 Scene View（Tilemap）之间的定位与过渡；支持在 Map 上进行边上插值，同时在 Scene 中做入场/离场。
- 节点几何（Map 级）：
  - `LocationNode.pos(x,y)` 作为世界坐标锚点；Map View 直接绘制；
  - `PathEdge.polyline` 可选用于非直线道路的渲染与长度估计。
- 代价/时间：
  - `edge.cost` 继续作为仿真用代价（可与几何长度不同）；
  - 可选采用 `cost = distance * factor` 的策略保持一致性（由生成器或工具链写入）。
- 进度编码（Telemetry）：
  - 暴露 `movement_progress{entityId, from, to, t01}`，以 `t01` 在 Map 上插值 `polyline`；
  - GUI 不跨线程读 ECS。
- 节点→场景（Scene）桥接：
  - 在 `WorldAtlas::tilemaps` 中为节点定义 `portals/anchors`：`{ edgeId|toId, anchor: {x_local,y_local} }`；
  - Scene View 利用锚点将 Map 边进度映射为局部坐标，实现入场/离场；资源/交互点由 Tilemap 的 ObjectLayer 提供。

## Demo 世界（最小例）
`data/world/demo_world.json` 含：
- 地点：区域（Town Center）、建筑（Tavern/Residential）、房间（Kitchen/Common Hall/Dormitory）
- 边：区域↔建筑，建筑↔房间；厨房为单向边（仅从 Tavern→Kitchen）
- 资源点：
  - Kitchen@3：Food，`capacity=24`，`rate_per_step=3`
  - Common Hall@4：Social，`capacity=12`，`rate_per_step=2`

## 相关文档
- 世界生成设计：`docs/architecture/WORLD_GENERATION.md`
- 图到矩阵映射（CLI 可视化）：`docs/architecture/GRAPH_TO_GRID.md`
