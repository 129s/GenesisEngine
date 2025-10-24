# 世界数据契约迁移指南（v1 LocationGraph → v2 WorldDatabase）

目标：从旧的基于节点/边（LocationGraph）的世界表达迁移到 v2 的 `WorldDatabase` 契约，采用“world.json + map_{id}.json”目录结构，支持 Map 内直线移动与 Map 图跨图拼接。

## v2 数据产物
- 世界总表 `world.json`：
  - `maps[]: { id:uint32, name:string }`
  - `map_edges[]: { from:uint32, to:uint32, bidirectional?:bool }`
- 每图分表 `map_{id}.json`：
  - `scenes[]: { id:uint32, parent?:uint32, name:string }`
  - `interactions[]: { id:uint32, sceneId:uint32, kind:string, coord:[int,int], capacity?:uint32, regen?:uint32 }`
  - `portals[]: { interactionId:uint32, targetMapId:uint32, targetSceneId?:uint32, targetCoord?:[int,int] }`
  - `tilemap?`: 渲染层元数据（与运行时解耦）

说明：
- 资源交互（`kind=Resource`）可直接在 `interactions` 中声明 `capacity` 与 `regen`（每步再生量）。
- 跨图连接通过 `map_edges` 表达；如需 `cost/rules` 等高级字段，可在后续 schema 扩展时加入。

## 迁移映射
- v1 Location → v2 Interaction：
  - 将能被“停留/交互”的节点转为 `Interaction(kind=Landmark|Resource|Portal|...)`，并确定 `sceneId` 与 `coord`（整格/世界坐标）。
  - v1 的资源产点（spawns）转换为 `Interaction(kind=Resource)`，同步迁移 `capacity/rate` → `capacity/regen`。
- v1 Edge → v2 MapEdge/Portal：
  - 跨区域/楼层/地图的连通转换为 Map 层 `map_edges`；场景入/出锚点转为 `portals[]` 与对应的 `interactionId`。
  - v1 边的几何/锚点（polyline/anchors）仅用于渲染可视化，可择机写入 `tilemap/meta`，运行时不读取。

## 迁移步骤
1) 建立 `maps[]` 与 `map_edges[]`：将原区域/楼层划分为 Map；按可达关系建立有向边，必要时标注 `bidirectional=false`。
2) 为每张 Map 建立 `map_{id}.json`：梳理 `scenes[]`（分组/布局），把可交互处转为 `interactions[]` 并填入 `coord`。
3) 对资源交互补齐 `capacity/regen`；Portal 转为 `portals[]` 并声明目标定位（Map/Scene/Coord）。
4) 用最小样例跑通 GUI Atlas/实体直线移动；逐步扩充数据与渲染元。

## 最小示例（v2）
world.json
```json
{
  "maps": [ { "id": 1, "name": "Town" } ],
  "map_edges": []
}
```
map_1.json
```json
{
  "scenes": [ { "id": 100, "name": "Town Center" } ],
  "interactions": [
    { "id": 10001, "sceneId": 100, "kind": "Resource", "coord": [10, 5], "capacity": 24, "regen": 3 }
  ],
  "portals": []
}
```

## 校验建议
- ID 唯一性与引用合法性（sceneId/interactionId/mapId）。
- 坐标范围与数据类型（`coord` 为整数对）。
- 资源参数非负且 `current<=capacity`（运行时默认填满）。
