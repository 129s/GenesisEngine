# 世界表示 · 分层节点图 vs Tilemap

## 分层节点图（World Graph）职责
- 结构
  - 节点：`Region/Building/Room/Point`（可扩展 Kind），支持父子关系表示层级。
  - 边：带 `cost` 和 `bidirectional`，表达通行/代价/方向。
  - 资源点：绑定到节点（通常是 `Point/Room`），有 `capacity/rate/type` 等属性。
- 能力
  - 全局路径规划：跨区域/建筑/房间的路径搜索（Dijkstra/A*）。
  - 资源选址：在约束（类型、容量、距离）下挑选满足条件的节点。
  - 生成/加载：从文件（JSON）或生成器（如 Noise）构建图；提供 `locations()/edgesFrom()/allSpawns()`。
- 输出
  - `WorldAtlas`：面向 UI 的只读布局：节点/边/资源位置 + 预排版坐标（用于静态渲染）。

## Tilemap（局部栅格）职责
- 结构
  - 与 `Room/Point` 绑定的 2D 网格：瓦片类型、阻挡、装饰、交互点（例如门/柜台）。
  - 门洞/传送口：连接邻接 Tilemap 或映射到图边，提供入/出锚点。
- 能力
  - 局部寻路：细粒度网格（如 4/8 邻接）进行短途避障或站位计算。
  - 视觉/渲染：GUI 的局部视图（可选），或导出给游戏前端。
  - 交互：精细的拾取/占位/范围查询（供系统/前端选择性使用）。

## 协同与边界
- 边界约束
  - 规划：Graph 负责“去哪儿”，Tilemap 负责“怎么走到锚点/门洞”。
  - 同步：当 Graph 拓扑或 Tilemap 布局变更，应提升 `world_version`，触发 Atlas 重建/前端刷新。
- 数据传递
  - Runtime → UI：仅通过 `Telemetry + Atlas`；UI 不跨线程访问 ECS。
  - Runtime 内部：Graph 与 Tilemap 通过 Portal/锚点协议（统一坐标系或局部坐标 + 映射）对接。

