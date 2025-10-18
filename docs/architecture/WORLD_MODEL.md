# 世界模型（World Model）

世界模型由两部分组成：**分层节点图（LocationGraph）** 与 **Tilemap（表/里视图）**。前者服务于逻辑运行与宏观寻路，后者负责渲染、局部寻路与交互定位。本节说明运行时代码依赖的契约、数据结构与加载流程。

## LocationGraph：逻辑骨架
- **节点层级**
  - `Region`：宏观区域（城市、森林、山谷），承载全局主题与配置。
  - `District`：区域内的功能分区（商业街、居住区、密林、河岸）。
  - `Area`：可进入的局部空间（室内场景、森林空地、洞穴、道路段）。
  - `Anchor`：在某张 map 中的定位锚点，用于入口/站位/Portal 出入口。
  - `Interaction`：交互节点（资源点、任务点、特殊设施）。
  - `Portal`：一类特殊 `Interaction`，表示跨 map/area 的通路（地铁入口、洞穴口、门）。
- **节点字段（建议）**
  - `id:uint32`
  - `parent:uint32`（树结构：Region→District→Area→Anchor/Interaction/Portal）
  - `kind:LocationKind`（Region/District/Area/Point…）
  - `role:NodeRole`（`anchor` / `interaction` / `portal` 等，仅在 Point 层使用）
  - `mapId:string`（所属 Tilemap，Region/District 若无 Tilemap 可设为空）
  - `coord_local:{x:int,y:int}`（在所属 map 内的格子坐标；无 Tilemap 时留空）
  - `metadata`：可扩展字段，如容量上限、Portal 宽度、Trait 限制等。
- **边与 Portal**
  - `PathEdge{ from, to, cost, bidirectional, portalNode? }`
  - 每个 Portal 节点应在 `metadata` 中记录 `{ targetNodeId, targetMapId, targetCoord }`，方便运行时查找跨图入口。
- **资源节点**
  - 运行时通过 `NodeRole=interaction` + `interactionType`（如 Food, Social, Workshop）识别。
  - 资源系统在加载时为这些节点挂载 ECS 组件（库存、产出速率等）。

## Tilemap：表/里视图
- 每个 `mapId` 对应一组资源：
  - `outsideView`：当该 map 作为子场景挂载在父 map 中时的呈现方式，可为单层贴图或缩略视图。
  - `insideView`：代理/摄像机进入该 map 时加载的完整 Tilemap，包含多层瓦片、碰撞、装饰。
  - `children[]`：声明子 map 及其入口位置，供生成器与运行时建立 Portal。
- Tilemap 的对象层应标注：
  - `Anchor` 对象：`{ nodeId?, role:"anchor", coord }`
  - `Interaction` 对象：`{ role:"interaction", type:"food", coord }`
  - `Portal` 对象：`{ role:"portal", toMapId, toCoord, width, passMask }`
  - 若生成器已分配 nodeId，可写入；否则运行时加载后回填。
- 资源格式支持 JSON (`.tmj`) 与二进制（规划中），详细见 `sandbox_gui_tilemap_rendering.md`。

## 运行时加载流程
1. **读取 Graph 与 Tilemap 元数据**：
   - 世界生成器输出 `world.json`（LocationGraph）与 `tilemaps/*.tmj`（或二进制）。
   - `WorldLoader` 解析 JSON 创建节点与边，构建 `LocationGraph`。
   - `TilemapLoader` 解析 Tilemap，提取 inside/outside 元数据、对象层标记。
2. **装配 WorldRegistry**：
   - `WorldRegistry.setGraph(LocationGraph)`：建立节点、边、Portal 索引。
   - 校验：树结构完整、Portal 指向存在、Anchor/Interaction 坐标合法。
3. **同步 Tilemap 信息**：
   - `WorldAtlasBuilder` 读取 `TilemapMeta`，将 mapId、dimensions、children、format 等写入 Atlas。
   - 为每个 Anchor/Interaction 节点填充 `mapId` 与 `coord_local`（若未在 JSON 中提供）。
4. **注册运行时组件**：
   - 资源节点：创建 `ResourceSpawn`、`ResourceInventory` ECS 组件。
   - Portal：建立 `PortalIndex`，加速跨 map 寻路与 GUI 渲染定位。

## 运行时查询
- `WorldRegistry` 提供：
  - `node(id)`、`children(id)`、`ancestors(id)`；
  - `anchorsOf(areaId)`、`interactionsOf(areaId)`、`portalsFrom(nodeId)`；
  - `findPortalBetween(mapA,mapB)`、`edgesFrom(nodeId)`。
- `TilemapRegistry` / `WorldAtlas` 提供：
  - `tilemap(mapId)` 元数据（inside/outside 路径、尺寸、格式）；
  - 锚点/Portal/交互点在 Tilemap 中的坐标；
  - `world_version`，用于前端缓存刷新。

## 宏观与局部寻路
- **逻辑寻路**：在节点图上寻找 `Portal` → `Anchor` 链，决定宏观目标。
- **局部寻路**：当需要渲染时，根据 `mapId + coord_local` 在 Tilemap 上求细粒度路径。
- Portal 作为特殊交互节点参与两者，确保逻辑与渲染对齐。

## 数据示例
```json
{
  "locations": [
    { "id": 1, "kind": "Region", "name": "NewTown", "parent": 0 },
    { "id": 10, "kind": "District", "name": "MainStreet", "parent": 1, "mapId": "main_street" },
    { "id": 101, "kind": "Area", "name": "Tavern", "parent": 10, "mapId": "tavern_interior" },
    { "id": 1011, "kind": "Point", "role": "anchor", "name": "TavernDoor", "parent": 101,
      "mapId": "tavern_interior", "coord_local": [4, 12] },
    { "id": 1012, "kind": "Point", "role": "interaction", "interactionType": "food",
      "name": "KitchenCounter", "parent": 101, "mapId": "tavern_interior", "coord_local": [8, 6],
      "metadata": { "capacity": 24, "regenPerStep": 3 } }
  ],
  "edges": [
    { "from": 10, "to": 101, "cost": 4.0, "bidirectional": true, "portalNode": 1011 }
  ]
}
```

## 相关文档
- 世界生成与 MapConfig：`world_generation.md`
- Graph ↔ Tilemap 协同：`world_representation.md`
- Tilemap 管线与渲染：`sandbox_gui_tilemap_rendering.md`
