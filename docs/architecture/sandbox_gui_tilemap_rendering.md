# Sandbox GUI · Scene View 渲染计划

目标：在 Sandbox GUI 中提供基于 Tilemap 的 Scene View，可视化 MapConfig 生成的 inside/outside 视图、Portal/锚点，并绘制代理的局部路径；前端严格遵守“只读 Telemetry/Atlas”的并发边界。

## 1) 范围与目标
- 展示：渲染当前 map 的 insideView（完整瓦片层）与可选的 outsideView 缩略图。
- 交互：平移/缩放、网格开关、图层可见性、Portal/锚点高亮；支持从 MapView 选中节点跳转。
- 代理：使用 Telemetry 的 `mapId + localPosition` 绘制 NPC，并在 Portal/动作切换时平滑过渡。
- 性能：视口裁剪、纹理缓存、LRU、批次绘制；Atlas 更新时自动刷新资源。

非目标（暂缓）：Tile 动画、实时编辑器、动态瓦片修改、光照/粒子。

## 2) 世界与数据表达
- Graph ↔ Tilemap 协议见 `world_representation.md`：Scene 节点提供 `coord_global`，Interactive 节点（resource/portal 等）提供 `coord_local`；Portal 属于 Interactive，`PathEdge.anchors` 给出进出锚点。
- `TilemapMeta`（Atlas 字段）包含：
  - `mapId`
  - `outsideView { format, path, bounds }`
  - `insideView { format, path, width, height, tileSize, layerCount }`
  - `children[] { childMapId, portalNode }`
  - `binary { enabled, preferredFormat }`
- 对象层记录 `anchor/interaction/portal` 标签，GUI 用于与 `WorldRegistry` 数据对齐并做高亮。

## 3) Runtime API 合同（增量）
- Atlas 扩展：
  - `WorldAtlas::tilemaps[]` 提供 inside/outside 视图元数据、格式、子 map、Portal 列表。
  - `WorldAtlas::nodes` / `portals` 暴露 Scene/Interactive 节点，包含 `coord_global/coord_local` 与 Portal 锚点，Scene View 用于落点与线路绘制。
- Telemetry：
  - `agents[]`：`mapId`, `localPosition`, `movementProgress`, `traits`。
  - `movementProgress`（0..1）用于从 MapView 边插值到 SceneView 入口。
- 并发：Atlas/Telemetry 均带 `world_version`；GUI 在版本变更时刷新缓存，不跨线程访问 ECS。

## 4) 资产与加载
- 格式：优先 Tiled `.tmj`；提供转换脚本生成二进制 `.tmb`（Header + LayerBlock + ObjectBlock，详见“二进制协议”段）。
- 目录结构：`data/tilemaps/<mapId>/{inside.tmj, outside.tmj}` 或 `.tmb`；Tileset 图集位于 `data/tilesets/`。
- Loader：
  - Runtime 仅解析元信息（尺寸、层数、对象标记），构建 `TilemapRegistry`。
  - GUI 线程按需加载纹理；若格式为 `.tmb` 则调用二进制解析器直接映射压缩数据。
- 缓存：使用 `TilemapCache` 管理纹理与层数据，LRU 回收长时间未使用的 map。

## 5) 渲染器设计（GUI）
- 纹理：
  - OpenGL + stb_image（或二进制解码）加载 tileset，生成 `ImTextureID`，缓存到 `TextureCache`。
  - 多 tileset 支持：每个 layer 记录 tilesetId，渲染时查缓存。
- 瓦片绘制：
  - `ImDrawList::AddImageQuad`/`AddImage` 逐层渲染，计算 `uv`。
  - 视口裁剪：根据当前相机矩形，仅绘制可见 tile 范围；大图可拆 chunk 以减小循环。
  - 坐标系：左上 (0,0)，Y 向下；提供 `pan/zoom`、适配 DPI。
- 覆盖与调试：
  - ObjectLayer 渲染线框/Portal 箭头；可选显示 Anchor、Interaction 标签，并对齐 `WorldRegistry` 中的 `coord_global`。
  - 网格开关、坐标拾取（调试）。
  - Agents：将 Telemetry `localPosition` 转屏幕坐标绘制标记，显示名称/当前行动。
- 面板集成：
  - Scene View 面板：下拉选择 mapId 或跟随 MapView 选中；支持 inside/outside 切换。
  - 图层列表、可见性开关、网格按钮、重置视图、Portal 高亮。

## 6) 与 Map View 的联动
- MapView 选中 `Area/Portal` 节点 → SceneView 切换到对应 map 并聚焦锚点。
- SceneView 点击 Portal → MapView 高亮对应边，支持“一键跳转到目标 map”。
- 当代理在 SceneView 可见范围内移动时，将其路径与 Portal 进度同步显示。

## 7) 性能与内存
- MVP：基于裁剪的逐 tile 绘制即可满足 64×64 / 32px 瓦片场景。
- 中期：
  - 静态层烘焙（FrameBuffer 缓存），滚动时采样子矩形。
  - 分块（chunking）+ LRU 纹理页，支撑大地图。
  - GPU 合批（实例化、图集 atlas 合并）作为后续优化项。

## 8) 里程碑拆解
- **M1** 静态渲染：加载 insideView，完成裁剪、TextureCache、基础 UI。
- **M2** 多层/对象层：支持多 tileset，ObjectLayer 渲染，Portal/Anchor 高亮。
- **M3** Agents 覆盖：接入 Telemetry `localPosition`，按缩放调整标记。
- **M4** 联动：与 MapView 同步、Portal 跳转、inside/outside 切换。
- **M5** 性能打磨：LRU 缓存、静态层烘焙、二进制格式解析、UX 细节。

## 9) 接口与数据结构（建议）
- C++（Atlas 侧）
  ```cpp
  struct TilemapMeta {
    std::string mapId;
    TilemapView outsideView;
    TilemapView insideView;
    std::vector<ChildMap> children;
    bool binaryEnabled;
  };
  struct TilemapView {
    std::string format; // "tmj" | "tmb"
    std::string path;
    int width, height;
    int tileSize;
    int layerCount;
  };
  ```
- Telemetry：
  ```cpp
  struct AgentSnapshot {
    uint32_t entityId;
    std::string mapId;
    Float2 localPosition;
    MovementProgress progress;
    std::string currentGoal;
  };
  ```

## 10) 二进制格式（规划）
- 容器结构：`Header(魔数, version, mapIdLen)` → `TilesetBlock` → `LayerBlocks` → `ObjectBlock`。
- Tile 层数据采用 RLE/LZ4 压缩，支持随机访问；ObjectBlock 存储 Portal/Anchor/Interaction 元数据。
- `WorldAtlas::tilemaps[].binary.enabled=true` 表示可优先使用 `.tmb`；CLI 或调试仍可读取 `.tmj`。
- 优先级：中，待现行 JSON 管线稳定后实现。

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
