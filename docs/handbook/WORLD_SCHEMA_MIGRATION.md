# 世界数据契约迁移指南（LocationGraph → 含坐标/锚点）

目标：将现有世界数据升级到新的必备契约，支持 Map/Scene 一致渲染与过渡。

## 必备字段（重构后）
- `locations[i].coord_global: [int,int]`（Map 全局网格坐标）
- `edges[i].anchors: { at_from:[int,int], at_to:[int,int] }`（Scene 入/出场锚点，局部坐标）
- `spawns[i].local_coord: [int,int]`（Scene 资源/交互点局部坐标）
- 可选：`edges[i].polyline: [[int,int], ...]`（Map 边几何）

## 迁移步骤
1) 为每个 `locations[i]` 指定 `coord_global`（建议使用网格化编辑或简单脚本分配）。
2) 为每条 `edges[i]` 指定 `anchors`：
   - `at_from` 是 `from` 节点所属 Scene 的局部坐标；
   - `at_to` 是 `to` 节点所属 Scene 的局部坐标；
   - 如节点未绑定 Scene，可将锚点设置为占位局部中心（但仍需提供数值）。
3) 为每个 `spawns[i]` 指定 `local_coord`（资源在 Scene 的局部位置）。
4) 设置 `schema_version`（与 Runtime/GUI 配置一致）。
5) 用最小示例跑通 Map/Scene 渲染后，再完善实际大图数据。

## 最小示例片段（节选）
```json
{
  "schema_version": 1,
  "locations": [
    { "id": 1, "parent": 0, "name": "Town Center", "kind": "Region",   "navigable": true,  "coord_global": [10, 8] },
    { "id": 2, "parent": 1, "name": "Tavern",       "kind": "Building", "navigable": true,  "coord_global": [12, 9] },
    { "id": 3, "parent": 2, "name": "Kitchen",      "kind": "Room",     "navigable": true,  "coord_global": [13, 10] }
  ],
  "edges": [
    { "from": 1, "to": 2, "cost": 1.0, "bidirectional": true,
      "anchors": { "at_from": [5,5], "at_to": [2,6] } },
    { "from": 2, "to": 3, "cost": 0.5, "bidirectional": true,
      "anchors": { "at_from": [3,2], "at_to": [4,4] } }
  ],
  "spawns": [
    { "name": "Tavern Food Prep", "type": "Food", "location": 3,
      "capacity": 24, "rate_per_step": 3, "local_coord": [6,4] }
  ]
}
```

## 工具与校验建议
- 使用脚本检查所有节点/边/资源是否具备上述字段。
- 对 `coord_global` 做去重/越界校验；对 `anchors/local_coord` 做 Scene 范围校验。
- 可在导入器中加入 schema 校验并在日志中报告缺失字段。

