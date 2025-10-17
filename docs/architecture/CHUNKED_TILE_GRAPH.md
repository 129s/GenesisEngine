# 噪声世界的区块包含与节点图描述

> 目标：在 Terraria/Noita 类噪声生成世界（Tilemap）中，使用“节点图”作元结构，表达“区块（Chunk）包含子区块/区域”的层级与连通，支持流式加载与寻路。

## 核心思路
- 分层模型：
  - 语义/拓扑层（节点图）：描述“有哪些区域/子区域/门户、它们如何相连、通行代价是多少”。
  - 几何/细节层（Tilemap）：真实格子、材质、障碍/流体等由 Tile 引擎维护。
- 节点图不存几何细节，只引用 Tile 空间的“范围/入口”作为锚点，实现轻量抽象与跨区寻路。

## 层级与节点类型（建议）
- Node.kind（可扩展枚举或标签）：
  - `Region/Biome`：大区或生物群系（可选）
  - `Chunk`：基础加载单位（如 64×64 tiles）
  - `Area`：行走可达的连通分量（由 Flood-Fill 从 Tilemap 提取）
  - `Room/Cave`：`Area` 的语义子类（可选）
  - `Portal`（可选为独立实体）：连接两个 `Area` 的边界门/缝/缆索点
- 必备属性：
  - `bounds`：Tile AABB（`minX,minY,maxX,maxY`），或对不规则形状采用 RLE/多边形引用（后续）
  - `passability_mask`：可通行能力位（走/跳/游/飞等），用于过滤边
  - `material/hazard`：主要材质/危险度标签（噪声域反映）
  - `parent`：父节点（实现“区块包含”）

## 边与“门户”
- `PathEdge`（高层边）：`from`/`to` + `cost` + `bidirectional`
- `Portal`（推荐显式化）：
  - 定义：`{ id, a:NodeId, b:NodeId, a_tile:(x,y), b_tile:(x,y), width:int, pass_mask, base_cost }`
  - 用途：在 Tile 层做局部 A* 的起止锚点；在图层做跨区边的几何依据
- 边权构成：基础代价 + 地形/危害加成 + 垂直移动（跳跃/爬升）代价等

## 从噪声 Tilemap 提取图（离线/在线）
1. 噪声采样 → Tile 分类：固体/半固体/平台/液体/空洞/危险
2. Passability 规则：按代理能力（走/跳/游/飞）确定“可达 Tiles”
3. 组件标记：对可达 Tiles 做连通域标记（Union-Find/Flood-Fill），得到 `Area` 节点；记录其 `bounds` 与属性统计
4. 邻接/门户检测：在相邻 Chunk 边界和 `Area` 边界，检测连续可通行的“门段”（seam run）；
   - 将长门段采样成若干 `Portal`（按最大间距或转角处）
   - 为每个 `Portal` 建立 `PathEdge(a,b)`，并附几何锚点
5. 层级绑定：
   - `Area.parent = Chunk`，`Chunk.parent = Biome/Region`（按噪声标签/分区）
6. 图规约（可选）：
   - 收缩度=2 的中间节点（线段化）、保留关节点/枢纽，提高寻路效率

## 区块加载与拼接（流式）
- Chunk 生成/加载：
  - 内部生成其 `Area` 集与内部 `Portal`；
  - 对上下左右邻接 Chunk 的边界做“缝合”：匹配边界 run，把两侧 `Area` 通过 `Portal` 连接（或合并为同一个连通域，需跨 Chunk 的 union-find 映射）
- 跨 Chunk 连通域合并：
  - 用“边界代表”表延迟合并；当相邻 Chunk 到齐后再统一 id（或保持别名映射）
- 卸载策略：
  - 仅保留邻接门户与跨 Chunk 映射的最小索引；内部 `Area` 可丢弃细节，仅保留摘要（tile 范围与度量）

## 节点图与 Tile 的映射
- 位置查询：
  - `tile(x,y) → AreaId`（按 Chunk 内部 2D id 缓存，O(1) 查找）
- 行为落点：
  - 资源/交互点：在 `Area` 中定义一组 anchor tiles（或从噪声/装饰规则推导）
- 寻路：
  - 图层：`A*` 在 `{Area,Portal}` 上求出高层序列
  - Tile 层：逐段以 `Portal` 的 `(x,y)` 为起止做局部 `A*`

## 数据/接口草案（增量）
- Node 扩展：
  - `struct SpatialBounds { int minX, minY, maxX, maxY; }`
  - `struct LocationNode { ..., SpatialBounds bounds; uint32_t passMask; std::string biome; }`
- Portal 实体：
  - `struct Portal { uint32_t id; LocationId a, b; Int2 aTile, bTile; int width; uint32_t passMask; float baseCost; }`
  - 边指向 Portal：`PathEdge{ from:a, to:b, cost, portalId }`（portalId 可选）
- 运行时：
  - `AreaIndex`：Chunk 局部 `tile→AreaId` 映射；
  - 跨 Chunk 合并表：`UnionAlias{ localAreaId → globalAreaId }`

## 适配现有代码的演进
- 保留现有 `LocationGraph/WorldRegistry` 接口，按需扩展 `LocationKind` 与节点属性（`bounds/passMask`）。
- 引入 `Portal` 集合与 seam 拼接流程；`WorldRegistry` 提供 “根据 tile 位置查找 Area/Portal” 的服务。
- MovementSystem：分层寻路与执行；`AgentTransform{ location, tileX, tileY }` 替换/扩展现有位置组件。
- Telemetry：可选增加 `tileX,tileY`、当前 `AreaId/PortalId`，便于可视化与诊断。

## 约束与校验
- 约束：`PathEdge` 必须有可用 Portal；Portal 两端 tile 必须可达且非障碍；
- 校验：构建/加载时对边界和门户做一致性检查；图规约前后保持同构性（可达集不变）。

## 小结
- “区块包含”的层级关系通过 `parent` 链条体现在节点图；
- “跨区连通”通过 `Portal + PathEdge` 描述；
- 图层轻量、Tile 层细节，二者配合实现高效流式寻路与渲染。