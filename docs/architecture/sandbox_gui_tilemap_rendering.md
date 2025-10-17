# Sandbox GUI · Scene View（原 Tile View）渲染计划（原型参考实现）

目标：在 sandbox_gui 内新增 Scene View（基于 Tilemap 的主观察视图）与交互，作为后续游戏场景的原型与参考实现；坚持“前端只读 Telemetry/Atlas，不跨线程访问 ECS”的并发边界。

## 1) 范围与目标
- 展示与调试：在 GUI 中可视化与某些 Room/Point 绑定的局部 Tilemap（瓦片布局、阻挡与门洞）。
- 交互与导航：提供平移/缩放/网格开关、图层可见性切换；支持从 Map View 点击节点跳转到对应 Scene View。
- Agents 覆盖：在具备本地坐标/进度的前提下绘制 Agents 的局部位置；缺失本地信息视为数据错误，Scene 不渲染该实体。可选扩展：使用 `movement_progress` 做边上插值。
- 性能与工程：视口裁剪、纹理缓存、简洁接口；不引入 ECS 跨线程读。

非目标（后续扩展）：
- Tile 动画、动态修改瓦片、粒子/光照；复杂 UI 编辑器。

## 2) 世界与数据表达
- Graph + Tilemap 双轨：
  - Graph 负责全局路径与资源分布；Tilemap 负责局部碰撞/落点/渲染。
  - Portal（门洞/锚点）映射 Graph 边与 Tilemap 入口出口。
- Tilemap 描述（只读）：
  - `TilemapDescriptor { nodeId, width, height, tileSize, layers[], tilesets[] }`
  - Layer 支持：`TileLayer`（瓦片索引二维阵列）、`ObjectLayer`（碰撞多边形/门洞/交互点）
  - Tileset：单图集起步（atlas.png + tile rect 列表），后续支持多图集。
- 注册表：
  - `TilemapRegistry` 挂在 World 内部（加载/生成时填充）；Runtime 导出给 GUI 时只提供只读快照或 Atlas 扩展（见下）。

## 3) Runtime API 合同（增量）
- Atlas 扩展：
  - `WorldAtlas::tilemaps: [{ nodeId, width, height, tileSize, layersMeta, tilesetMeta }]`
  - GUI 可据此决定是否能渲染某节点的场景视图（无需 ECS）。
- Telemetry 扩展（可选，供插值/覆盖）：
  - `movement_local: [{ entityId, nodeId, x, y }]`（局部坐标，像素或格）
  - `movement_progress: [{ entityId, from, to, t01 }]`（图边行进进度 0..1，用于从 Map→Scene 的插值桥接）
  - 若无该字段，GUI 仅按节点中心绘制代理。

并发与一致性：
- Atlas/Telemetry 附带 `world_version`，版本变更时 GUI 侧重建/刷新；不跨线程读 ECS/Registry。

## 4) 资产与加载
- 格式选型：优先 Tiled JSON（.tmj）以获得生态；第一阶段可用内置 JSON 简化（减少依赖）。
- 数据路径：`data/tilemaps/<node_or_room_name>.tmj` 与 `data/tilesets/*.png`
- Loader：在 Engine/WorldLoader 阶段解析，构建 `TilemapRegistry` 与 `WorldAtlas::tilemaps` 元数据；纹理只在 GUI 侧加载。

## 5) 渲染器设计（GUI）
- 纹理：
  - 使用 OpenGL 载入 PNG（stb_image），以 `ImTextureID` 交给 ImGui；建立简单 `TextureCache`（按 tileset 路径缓存）。
- 瓦片绘制：
  - 使用 `ImDrawList::AddImageQuad`/`AddImage`，根据 tile 索引计算 `uv`，逐层绘制；
  - 视口裁剪：根据相机矩形，仅绘制可见 tile 的子区域；
  - 座标系：以 `origin` 左上为 (0,0)，Y 向下；相机提供 `pan/zoom` 与矩阵变换。
- 覆盖与调试：
  - `ObjectLayer` 渲染线框（碰撞、多边形/门洞）；
  - 可选网格与坐标显示；
  - Agents 覆盖：将 Telemetry 中 `movement_local` 转为屏幕坐标绘制标记。
- 面板集成：
  - 新增 “Scene View” 面板：下拉选择节点或跟随 Map View 选中；
  - 图层列表（多选显示）、网格开关、缩放滑条、重置视图按钮。

## 6) 与 Map View 的联动
- 选中 Graph 中的 Room/Point 节点时，Scene View 自动切换到对应 tilemap；
- Portal 可视化：在 Scene View 中高亮出入锚点（并显示连接的目标节点名）。

## 7) 性能与内存
- MVP：每帧快速裁剪 + 批量提交（每 tile 一次 AddImage 调用足够小图）。
- 中期优化：
  - 预烘焙静态层（离屏纹理/网格），滚动时仅采样子矩形；
  - 大图分块（chunk）+ LRU 纹理页；
  - GPU 端合批（后续可考虑）。

## 8) 里程碑拆解
- M1 静态渲染 MVP（1-2 天）
  - Atlas 扩展元数据（tilemap 存在与尺寸），GUI 侧 TextureCache 与加速相机/裁剪，单 tileset、单 TileLayer 渲染。
- M2 多层与对象层（1-2 天）
  - 支持多 TileLayer，`ObjectLayer` 线框；图层可见性 UI；网格开关。
- M3 Agents 覆盖（1 天）
  - Telemetry 增加 `movement_local`（或先由 Runtime 在进入 tilemap 时投影节点中心）；按 zoom 自适应标记大小与标签。
- M4 联动与 Portal（1-2 天）
  - Map View 选中节点 → Scene View 跟随；Portal 可视化与跳转。
- M5 性能与打磨（1-2 天）
  - 视口裁剪优化、纹理缓存、烘焙静态层；交互与 UX 细节。

## 9) 接口与数据结构（建议）
- C++（Atlas 侧）
  - `struct TilemapMeta { uint32_t nodeId; uint16_t width, height; uint16_t tileW, tileH; std::string tileset; std::vector<TileLayerMeta> layers; /*…*/ };`
  - `struct TileLayerMeta { std::string name; bool visible; std::vector<uint32_t> gids; /* row-major */ };`
- Telemetry（可选）
  - `struct LocalMovement { uint32_t entityId; uint32_t nodeId; float x, y; };`

## 10) 验收标准
- 在 demo 场景中可加载 64×64、32px 瓦片地图，60 FPS 以上；
- 图层可见性/网格开关/缩放平滑；
- Map View 与 Scene View 联动可靠；
- 不跨线程访问 ECS，Atlas/Telemetry 版本一致，UI 线程仅读。

## 11) 后续可选项
- Tile 动画；
- 贴图九宫格/自动连接；
- 复杂碰撞与 A* 局部寻路展示；
- 编辑器或外部工具链（Tiled）的一键导入脚本。
