# 世界模型（World Model）

本文定义“地图/场景/交互点/传送”四要素的概念与数据契约，明确运行时与渲染层的边界。目标：在保证表达力的前提下，以极低运行开销支持上千 NPC 并发。

## 决策摘要（v2）
- Map 图（有向）：世界由若干 Map 组成，Map 之间通过有向边（MapEdge）表征可达性与代价。
- Scene 树（分组/布局）：每张 Map 内部是 Scene 树，仅用于组织与坐标继承，不参与寻路。
- Interaction（交互点）：挂在 Scene 下的可交互锚点，包含 Portal/Resource/Workbench/Trigger 等，导航目标均指向交互点坐标。
- 导航语义：
  - Map 内部：NPC 以“直线”在交互点（锚点）之间移动，不做网格寻路/碰撞判定。
  - 跨 Map：通过 MapEdge（由允许的 Portal 组合形成）在 Map 图上最短路；进入/离开 Map 的入口/出口均为 Portal 对应的交互点。
- 渲染解耦：Tilemap 仅用于渲染表现；运行时不读取 Tilemap/碰撞。若需要阻断，请把区域拆分为不同 Map，并用 Portal/MapEdge 连接。

## 分层结构
1) 世界层（World / Map Graph）
   - `Map{id,name,meta?}`：可自由移动的最小连通区域。
   - `MapEdge{from,to,cost,rules?}`：Map 之间的有向连通（可按内容/脚本显式配置，非默认全连）。

2) 地图层（Per Map / Scene + Interaction）
   - `Scene{id,parent?,name,origin?,transform?,meta?}`：容器/坐标系承载，不产生导航节点。
   - `Interaction{id,sceneId,kind,coord_local,[coord_global]?}`：交互锚点。kind 包括 `Portal|Resource|...`。
   - `Portal{interactionId, channelId?, oneWay?, teleportCost?}`：Portal 是 Interaction 的一种，承载传送元。

3) 表现层（Rendering）
   - `Tilemap{width,height,tileW,tileH, ...}`：仅用于渲染的贴图/图层；可带引用将交互点投影到画面，但不影响运行时。

## 导航与移动
- Map 内：源交互点 → 目标交互点，按直线移动。
  - 代价：几何距离/速度（可叠加拥挤/人格偏好惩罚）。
  - 不存在“先去 Scene 再去 Portal”的中转，因为 Scene 不参与导航。

- 跨 Map（分层拼接）：
  1) 当前 Map：当前位置锚点 → 某出口 Portal 锚点（直线）。
  2) Map 图：在 `MapEdge` 上做最短路（Dijkstra/A*）；边权可包含传送基础费/冷却/权限等规则代价。
  3) 目标 Map：入口 Portal 锚点 → 目标交互点锚点（直线）。

- Portal 连边策略：
  - “任意两个 Portal 可以构成一条 Map 图上的 edge”意为“允许在规则满足时建立连边”，非默认全连。
  - 推荐显式列出 `MapEdge` 或按频道/白名单/邻近生成，避免 O(P^2) 边爆炸。

## 数据契约（最小集）
为便于实现，可以采用“世界总表 + 每图分表”的产物组织。以下为字段建议：

1) 世界总表 world.json（示例）
```json
{
  "maps": [
    { "id": 1, "name": "Town" },
    { "id": 2, "name": "Dungeon" }
  ],
  "map_edges": [
    { "from": 1, "to": 2, "cost": 5.0 },
    { "from": 2, "to": 1, "cost": 6.0 }
  ]
}
```

2) 每张图 map_{id}.json（示例）
```json
{
  "map": { "id": 1, "name": "Town" },
  "scenes": [
    { "id": 100, "name": "Town Center" },
    { "id": 200, "parent": 100, "name": "Tavern" }
  ],
  "interactions": [
    { "id": 10001, "sceneId": 100, "kind": "Resource", "coord": [10, 5], "capacity": 24, "regen": 3 },
    { "id": 10002, "sceneId": 200, "kind": "Portal",   "coord": [3,  7] }
  ],
  "portals": [
    { "interactionId": 10002, "channelId": "tavern-door", "oneWay": false, "teleportCost": 0.5 }
  ],
  "tilemap": { "width": 64, "height": 64, "tileW": 32, "tileH": 32 }
}
```

3) Agent 位置与移动（运行时快照建议）
```json
{
  "entityId": 123,
  "mapId": 1,
  "position": { "x": 12.5, "y": 7.0 },
  "movement": { "target": { "mapId": 2, "interactionId": 20001 } }
}
```

说明：
- 生产管线可将 `coord_local` 结合 Scene 原点转为 `coord_global`（可选缓存）；运行时最少只需统一整格/世界坐标即可直线移动。
- Portal 的跨图连接关系体现在 `map_edges`，而非“Portal 对默认全连”。

注：简化起见，v2 当前 `world.json` 的 `map_edges` 最小字段为 `{ from,to,bidirectional? }`；如需 `cost/rules` 可在后续 schema 扩展时加入。

迁移实施状态：仓库已移除旧的 LocationGraph/WorldRegistry/WorldLoader/WorldBootstrap 图模型与依赖，统一采用 WorldDatabase 与 Movement2D 运行时语义。所有新功能与测试请基于本文件定义的数据契约进行。

## 内容制作规范
- Map 语义：Map 内应“无阻挡、可直达”。若存在门/墙/楼层等阻断，请拆分为多个 Map，用 Portal/MapEdge 相连。
- Scene 作用：仅负责分组/布局与坐标继承；不要以 Scene 节点作为导航中转或碰撞代理。
- Portal 策略：
  - 仅在剧情/规则允许时为 Portal 组合建立 MapEdge。
  - 大规模 Portal 时使用频道/白名单/空间邻近阈值生成边，避免完全图。
- 坐标：统一整格/世界坐标；交互点的坐标是导航与渲染共同的锚点。
- 渲染：Tilemap 只管画面，变更不应影响运行时；若变更导致阻断，需相应调整 Map 划分与 Portal。

## 与现实现状的差异与迁移
- 差异：现实现将导航建立在“Location 节点/边”之上；本设计将导航语义转移到“Map 图 + 交互点直线移动”。
- 迁移建议：
  1) 在数据层补充 `mapId` 与 `Interaction` 概念（兼容旧格式，默认单 Map）。
  2) 在运行时继续支持旧的节点/边读取，但逐步将移动逻辑切换为“直线 + MapEdge 拼接”。
  3) GUI/工具优先改为以 Map/Interaction 为导航与高亮单位；Tilemap 仅作渲染。

## 术语表
- Map：可自由移动的最小连通区域。
- MapEdge：Map 层的有向连接与代价。
- Scene：分组/布局用的容器节点，不参与导航。
- Interaction：交互锚点（Portal/Resource/...），导航目标均指向此类实体。
- Portal：Interaction 的子类，承载传送元数据；跨图连通经由 MapEdge 表达。
- Tilemap：渲染用瓦片图，与运行时解耦。

——
相关文档：
- `world-representation.md`：渲染层契约（Tilemap 与可视化）。
- `world-generation.md`：数据生产流程与校验。
