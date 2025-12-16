# 世界表示（Rendering Representation）

本章界定“渲染层应消费哪些数据、如何绘制”，并明确其与运行时的边界。核心结论：Tilemap 仅用于表现，运行时不读取 Tilemap/碰撞；一切通行/阻断语义由 Map 划分与 Portal/MapEdge 表达。

## 作用与边界
- 渲染层消费 WorldAtlas（只读 DTO），绘制 Map、Scene 分组、Interaction/Portal 与可选 Tilemap。
- 运行时不依赖 Tilemap；Tilemap 改动不应改变 NPC 行为。若需要阻断，请在内容上拆分为多个 Map 并用 Portal/MapEdge 连接。

## WorldAtlas（渲染视图）
- 顶层：
  - `maps[]: { id,name,meta? }`
  - `mapEdges[]: { from,to,cost,rules? }`（用于可视化跨图连通，可开关显示）
- 每图：
  - `scenes[]: { id,parent?,name,origin?,transform?,meta? }`（层级标签，仅分组/布局）
  - `interactions[]: { id,sceneId,kind,coord_global }`（渲染锚点；kind=Portal/Resource/...）
  - `portals[]: { interactionId,channelId?,oneWay? }`（方向与频道用于箭头/分组）
  - `tilemap?: { width,height,tileW,tileH, ... }`（可选，仅表现尺寸/贴图引用）
- 版本：`world_version` 递增；前端据此刷新缓存。

## 坐标与映射
- 统一整格/世界坐标：`coord_global` 为渲染主坐标；如仅有 `coord_local`，通过 Scene 原点转换。
- 屏幕映射：`screen = worldToView * worldToPixels * coord_global`；Tile 原点通常在左上或左下，需在样式中规定。

## 绘制准则
- 绘制顺序：Tilemap（地面→装饰）→ Map 边/箭头（可选）→ Scene 边界（淡化）→ Interaction/Portal 图标 → Agent/标注。
- Portal 可视：
  - 频道/分组以颜色或形状编码；有向边以箭头标识。
  - 跨 Map Portal 在 Map 图视图上显示；同图 Portal 直接渲染为交互点。
- Agent 渲染：
  - 使用 Telemetry 中的 `mapId` 与 `position(x,y)`；必要时做插值/尾迹。
  - 跨图时隐藏当前 Map 的 Agent 标记，或以浮层提示“通过 Portal 移动中”。

## 交互与选择
- 选择单位是 Interaction/Portal；Scene 仅作为分组/过滤。
- Inspector 使用 Interaction ID 定位；可在 Atlas 上反查其 Scene 与 Map。

## 不做之事（渲染层）
- 不读取/计算碰撞或局部寻路。
- 不假定 Scene 节点是可站立/可通行的位置。

## 迁移
- 旧文档提到“分层寻路（Graph + Tilemap）”已弃用；新模型仅以 Map 图 + 直线移动为运行时语义，渲染层随之简化。
- 当内容需要复杂几何，请回到世界生成阶段，用 Map 划分与 Portal 表达。

## 参考
- 世界模型：`./world-model.md`
- 世界生成：`./world-generation.md`
- GUI（提案）：`../proposals/interface/sandbox/sandbox-gui-tilemap-rendering.md`
