# 噪声世界的区块抽象与节点图（存档/探索）

> 提示：本篇为历史探索稿，讨论“分层节点图 + Chunk 化 Tilemap + 局部寻路”的方案。当前 v1 世界模型采用“Map 图（有向）+ Scene 分组 + Interaction/Portal + Map 内直线移动”，运行时不读取 Tile 碰撞/不做局部寻路。若未来需要复杂几何和流式加载，可在此基础上重新评估。

## 核心思路
- 分层模型：  
  - 语义层（节点图）：`Region → District(Biome) → Area → Portal`，描述区块、语义与通路。  
  - 几何层（Tilemap + Chunk）：存储真实瓦片、材质、流体、破坏状态。
- 节点图不直接存格子，只引用 Tile 空间的范围/入口作为锚点；Portal 充当跨 Chunk 的桥梁。

## 层级与节点类型（建议）
- `Region` / `District`：宏观区域或生物群系（可映射到大地图）。
- `Area`：Chunk 内的连通分量（Flood-Fill 结果），可标记 `Surface`、`Cave`、`Underwater` 等语义。
- `Anchor`：在 Chunk 内的关键落点或标志物；供代理生成落脚点。
- `Interaction`：资源点、危险点、事件触发器等。
- `Portal`：特殊 `Interaction`，连接两个 Area，记录入口坐标与通行属性。
- 常用属性：
  - `bounds:SpatialBounds(minX,minY,maxX,maxY)`
  - `passMask:uint32`（可通行能力：走/跳/游/飞）
  - `hazard` / `biomeTag`（危险度、地形标签）
  - `parent` 指向上层节点（Chunk/District）。

## Portal 与边
- 逻辑边：`PathEdge{ from, to, cost, bidirectional, portalNode? }`
- Portal 结构：
  ```cpp
  struct PortalMeta {
    uint32_t nodeId;
    uint32_t fromArea;
    uint32_t toArea;
    std::string fromMapId;
    std::string toMapId;
    Int2 fromCoord;
    Int2 toCoord;
    int width;
    uint32_t passMask;
    float baseCost;
  };
  ```
- 边成本 = 基础代价 + 地形/危险加成 + 垂直移动系数。Portal 作为 `NodeRole=portal` 的节点参与寻路。

## 从噪声 Tilemap 提取节点图
1. 噪声采样与 Tile 分类：划分固体、平台、液体、危险等。
2. Passability 判定：依据代理能力过滤可通行 Tiles。
3. 连通域标记：Union-Find/Flood-Fill 生成 `Area`，记录 `bounds`、统计信息。
4. Portal 检测：在 Chunk 边界和 Area 边界扫描可通行带（seam run），按最大间距采样 Portal。
5. 层级绑定：`Area.parent = District/Chunk`，`District.parent = Region`；Anchor/Interaction 挂在对应 Area。
6. 图规约（可选）：压缩度=2 的中间节点、保留枢纽节点，提高寻路效率。

## 区块加载与拼接（流式）
- Chunk 加载时：
  - 生成内部 Area/Portal；
  - 与相邻 Chunk 的边界 Portal 尝试匹配，形成跨 Chunk 的桥接。
- 跨 Chunk 合并：
  - 使用 `UnionAlias` 将两侧 Area 合并为全局 id，或建立别名映射。
- Chunk 卸载：
  - 保留必要的 Portal/合并映射，释放内部 Area 细节，避免内存膨胀。

## 节点图与 Tile 映射
- `tile(x,y) → AreaId`：Chunk 内维护 O(1) 查询表。
- 锚点/交互点：在 Tilemap 对象层的 Anchor/Interaction 标记生成 Node，记录 `coord_local`。
- 寻路：
  - 逻辑层：在 `{Area, Portal}` 图上执行 A*。
  - Tile 层：沿逻辑路径逐段执行局部 A*（Portal 锚点 → 下一 Portal/交互点）。

## 数据与接口建议
- Node 扩展：
  ```cpp
  struct LocationNode {
    uint32_t id;
    LocationKind kind;      // Region/District/Area/Point
    NodeRole role;          // anchor/interaction/portal
    std::string mapId;
    Int2 coordLocal;
    SpatialBounds bounds;
    uint32_t passMask;
    std::string biome;
  };
  ```
- Runtime 结构：
  - `AreaIndex`：Chunk 局部 `tile → AreaId` 映射。
  - `PortalIndex`：加速查询“从当前 Area 可到达的 Portal”。
  - `UnionAlias`：跨 Chunk Area 合并表。

## 运行时代码适配
- 扩展 `LocationKind`/`NodeRole`，`WorldRegistry` 支持 Area/Chunk 查询。
- MovementSystem：
  - 宏观路径：依赖 Area/Portal；
  - 局部路径：在 Chunk Tilemap 上执行细粒度寻路；
  - `AgentTransform` 记录 `mapId + coord_local`。
- Telemetry：增加 `mapId`、`areaId`、`localPosition`，便于可视化追踪。

## 约束与校验
- Portal 两端必须可通行且存在目标 Area。
- 跨 Chunk 接缝需校验对齐：入口宽度一致、通行属性匹配。
- 图规约前后需保持可达集不变（同构或弱同构）。

## 小结
- Region/District/Area 层级支持大世界的语义与扩展。
- Portal 统一承担跨 Chunk 连通，既服务寻路也服务渲染。
- 通过 `AreaIndex + PortalIndex` 实现高效查询，图层轻量、Tile 层细节丰富。
