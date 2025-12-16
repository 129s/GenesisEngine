# Proposal · Sandbox GUI · Scene View Tilemap 渲染计划（对齐新世界模型）

目标：在 Sandbox GUI 中提供基于 Tilemap 的 Scene View，可视化每张 Map 的 Tile 层、Portal/交互点与代理位置；严格遵守“只读 Telemetry/Atlas”的并发边界。注意：运行时不读取 Tilemap/碰撞，GUI 仅渲染，不做局部寻路。

> 状态：提案（未作为当前主线实现目标）。当前可视化/线程边界请以 `docs/architecture/interface/sandbox/sandbox-gui.md` 与 `docs/architecture/foundation/runtime-api.md` 为准。

## 1) 范围与目标
- 展示：渲染当前 Map 的 Tile 层（insideView 等）与可选缩略图。
- 交互：平移/缩放、网格开关、图层可见性、Portal/交互点高亮；支持从 MapView 选中跳转。
- 代理：使用 Telemetry 的 `mapId + position(x,y)` 绘制 NPC，并在 Portal/动作切换时平滑过渡。
- 性能：视口裁剪、纹理缓存、LRU、批次绘制；Atlas 更新时自动刷新资源。

非目标：Tile 动画、实时编辑器、动态瓦片修改、光照/粒子、局部 A* 与碰撞计算。

## 2) 数据与契约
- 渲染协议见 [world/world-representation.md](../../../world/world-representation.md)：Atlas 提供 `maps/mapEdges` 与每图的 `scenes/interactions/portals[/tilemap]`；交互点坐标用于落点与高亮。
- `TilemapMeta`（Atlas 可选字段）：`{ width,height,tileW,tileH, ... }`；仅表现尺寸与资源引用。
- Telemetry：`agents[]` 暴露 `mapId` 与 `position(x,y)`；可选 `movement{from,to,t01}` 用于显示跨图段的过渡。

## 3) 资产与加载
- 格式：优先 Tiled `.tmj`；可选二进制 `.tmb`。
- 目录结构：`data/tilemaps/<mapId>/{inside.tmj, outside.tmj}` 或 `.tmb`；Tileset 图集位于 `data/tilesets/`。
- Loader：Runtime 仅解析元信息，GUI 线程按需加载纹理（或二进制解析）。
- 缓存：`TilemapCache` 管理纹理与层数据，LRU 回收未使用的 Map。

## 4) 渲染器设计（GUI）
- 纹理：OpenGL + stb_image/二进制解码生成 `ImTextureID`，缓存到 `TextureCache`；支持多 tileset。
- 瓦片绘制：
  - 逐层渲染（`ImDrawList::AddImage*`），计算 `uv`；
  - 视口裁剪：仅绘制可见 tile 范围；
  - 坐标系：左上 (0,0)，Y 向下；支持 `pan/zoom` 与 DPI 适配。
- 覆盖与调试：
  - 渲染 Portal 箭头与交互点图标；
  - 网格开关、坐标拾取；
  - Agents：将 Telemetry `position` 转屏幕坐标绘制标记，显示名称/当前行动。
- 面板：Scene View 下拉选择 mapId 或跟随 MapView 选中；图层列表/可见性/网格/重置视图/Portal 高亮。

## 5) 与 Map View 的联动
- MapView 选中 Portal/交互点 → SceneView 切换到对应 Map 并聚焦。
- SceneView 点击 Portal → MapView 高亮对应边，支持“一键跳转到目标 Map”。
- 当代理在 SceneView 可见范围内移动时，仅显示位置与方向提示，不做局部寻路。

## 6) 性能与内存
- MVP：裁剪的逐 tile 绘制满足 64×64 / 32px 瓦片。
- 中期：静态层烘焙（FBO 缓存）；分块 + LRU 纹理页；必要时 GPU 合批。

## 7) 里程碑
- M1 静态渲染：加载 insideView，完成裁剪、TextureCache、基础 UI。
- M2 多层/对象层：支持多 tileset，Portal/交互点高亮。
- M3 Agents 覆盖：接入 Telemetry `position(x,y)`，按缩放调整标记。
- M4 联动：与 MapView 同步、Portal 跳转、inside/outside 切换。
- M5 性能打磨：LRU、静态层烘焙、二进制格式解析、UX 细节。

## 8) 接口草案（Atlas 侧）
```cpp
struct TilemapMeta {
  int width, height;
  int tileW, tileH;
  // 可选资源引用...
};
```

## 9) 版本与线程
- Atlas/Telemetry 均带 `world_version`；GUI 在版本变更时刷新缓存。
- 不跨线程访问 ECS；UI 线程仅读取 Atlas/Telemetry。
