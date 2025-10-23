# 世界生成架构（World Generation）

目标：以 **场景拓扑 + 程序化布局 + 交互填充** 为核心构建全自动世界生成器，产出满足 [world/world-model.md](./world-model.md) 契约的 Scene/Interactive Node 树，并同步生成 Tilemap 等静态资产。生成结果必须完全可复现（`seed + 配置`），无需任何预制或编辑器介入。

## 核心原则
- **纯代码 + 配置化**：所有结构由生成器库计算得出，配置文件仅描述参数与规则启用，严禁手工摆放。
- **模块化流水线**：拓扑、布局、交互、验证、导出分层实现，可独立扩展与测试。
- **确定性**：同一输入在所有平台得到相同输出，方便重放与调试。
- **可缓存**：Tilemap 等重资产生成后可缓存复用，但缓存失效时必须能依靠代码重新生成。

## 1. 库边界与整合
- 新建 C++ 静态库 `genesis_worldgen`，在 CMake 中独立 target，提供头文件目录 `engine/worldgen`.
- 依赖仅限基础 STL、噪声/几何工具（内部实现），不引用渲染或运行时模块，避免循环依赖。
- 对外暴露纯 API：
  - `GeneratorConfig load_config(std::string_view path);`
  - `GeneratedWorld generate_world(const GeneratorConfig&, Seed seed);`
  - `void register_rule(const RuleDescriptor&);`
  - `TilemapCacheHandle` 等辅助接口。
- CLI/测试可直接链接该库，运行时 Loader 通过内存结构消费结果。

## 2. 模块划分

| 模块 | 职责 | 说明 |
| ---- | ---- | ---- |
| `TopologyModule` | 构建 Scene/Interactive 拓扑骨架（树/图结构） | 解析配置、决定层级、Portal 配对 |
| `LayoutModule` | 将节点映射到局部/全局几何坐标 | 执行不同布局策略，写入 footprint、层号 |
| `TilemapModule` | 依据布局生成/复用 Tilemap 资产 | 输出静态图层，提供缓存键和 metadata |
| `InteractionModule` | 填充资源点、Portal anchor、事件节点 | 结合玩法规则与场景标签分配 payload |
| `ValidationModule` | 对生成结果执行硬/软约束检查 | 包含可达性、容量、对称性、Tilemap 冲突等 |
| `ExportModule` | 序列化或落盘 | 生成 JSON、二进制快照或内存结构 |

模块之间通过 `WorldGenContext` 共享 deterministic RNG、日志、配置和缓存接口。

## 3. 数据流
1. **配置加载**：读取 JSON/TOML 配置，解析为 `GeneratorConfig` （主题、规模、规则集、实验开关）。
2. **初始化上下文**：根据 `seed` 创建高层 RNG，并派生给各模块；建立空 Node 树和 Tilemap 缓存索引。
3. **拓扑阶段**：
   - 规则组合生成 Scene/Interactive 草图：节点类型、父子关系、Portal 对、容量上限。
   - 对于每个集群入口，自动生成 `Portal(entry↔exit)` 草稿，后续布局与校验可据此补全锚点。
   - 输出 `SceneDraft` 列表，为布局阶段准备约束与节点元信息。
4. **布局阶段**：
   - 构造 `SceneLayoutContext`（边界、多边形、scene_capacity、保留区域、节点 footprint、layer 标记）。
   - 选择布局策略（grid/hex/noise/spline/cluster/voronoi/...），迭代放置节点。
   - 将局部坐标转换为全局坐标，并生成 `SceneLayoutResult`。
5. **Tilemap 生成**：
   - 针对已布局的每个 Scene，计算 Tilemap 缓存键（配置 + sceneId + seed）。
   - 未命中时调用策略生成 Tilemap（地形、遮挡、碰撞层），写入缓存并返回资产引用。
6. **交互填充**：
   - 根据 Scene 标签与 `scene_capacity` 分配资源节点、Portal 锚点、特殊交互。
   - 写入 `spawns[]`、`edges[]`、Portal `anchors` 等运行数据。
7. **验证**：
   - `ValidationModule` 执行硬约束（越界、间距、Portal 配对、节点唯一性、scene_capacity）以及软约束打分。
   - 对 Tilemap 做一致性检测（footprint 不与障碍冲突，可达性满足）。
8. **导出**：
   - 生成 `GeneratedWorld` 结构（locations/edges/spawns/tilemaps），附带调试日志与统计。
   - 根据调用者需求写入 JSON 快照或返回内存引用。

## 4. 配置结构
- 使用 TOML（或 JSON）描述，约定三层：
  - `world`: 全局参数（规模、主题、随机权重、可调范围）。
  - `topology/layout/interaction`: 规则集合及权重。
  - `experiments`: 实验性 Feature 开关。
- 示例摘录：

```toml
[world]
seed_offset = 0
scale = { min = 2, max = 5 }
theme = "frontier_outpost"

[topology.cluster]
min_clusters = 2
max_clusters = 4
min_nodes_per_cluster = 2
max_nodes_per_cluster = 5

[topology.corridor]
enabled = true
min_length = 2
max_length = 4

[layout.grid]
cell_width = 5.0
cell_height = 4.0
columns = 3
margin = 1.0

[layout.cluster]
radial_distance = 18.0
radial_step = 6.0
node_spacing = 1.5

[layout.corridor]
step = 7.0

[layout.hex]
enabled = true
spacing = 6.0

[layout.noise]
enabled = true
radius = 24.0
min_spacing = 3.5
max_attempts = 64

[interaction.resources]
food = { density = 0.3, capacity = [30, 60], rate = [1, 3] }
ore = { density = 0.2, cluster = "vein" }

[validation]
warnings_as_errors = false
min_connectivity = 1
```

- 配置加载后执行 Schema 校验；关键字段缺失直接报错。

## 5. 布局策略详解
布局策略实现统一接口 `LayoutResult layout(const SceneLayoutContext&, const NodeDraftList&, RNG& rng);`。当前计划首批实现：
- **GridLayout**：对齐网格，支持对齐、占位扩张、边界裁剪。
- **HexLayout**：蜂窝模式，满足等距与方向均衡。
- **NoiseRelaxationLayout**：噪声撒点 + Poisson/Lloyd 调整，生成自然分布。
- **SplineCorridorLayout**：沿样条曲线放置节点，适配道路/走廊/河道。
- **ClusterLayout**：生成中心簇，再调用其他策略布局子簇。
- **VoronoiRegionLayout**：依据种子点生成多边形区域，用于地块/势力划分。

策略可以组合：例如 ClusterLayout 内部调用 NoiseRelaxationLayout。

当前实现覆盖 `GridLayout`（簇内网格摆放）、`HexLayout`（蜂窝点阵）与基础走廊线性布置；其余策略按路线图逐步落地。

## 6. 约束模型
- **Scene 级硬约束**：`boundary`（多边形/矩形）、`scene_capacity`、`reserved_zones`、`connectivity_requirements`。
- **节点硬约束**：`footprint`（Rect/Radius）、`min_spacing`、`allowed_layers`、`orientation_lock`。
- **关系硬约束**：Portal 成对、楼层索引连续、必备节点存在。
- **软约束**：距离偏好、对称性、中心度等，以评分记录，可配置阈值。
- 布局阶段通过 `ConstraintSet::check_hard(candidate)` 即时判定，不满足立即回溯；软约束由 `ScoreEvaluator` 计算。

## 7. Tilemap 集成
- Tilemap 仅在生成阶段参与，运行时不参与逻辑。
- `TilemapModule` 读取 `SceneLayoutResult` 和 Scene 标签，生成静态 Tile 层：
  - 地形层、装饰层、碰撞层、导航掩码。
  - 输出 `TilemapDescriptor{ id, hash, size, layers[] }`。
- 缓存策略：`cache_key = hash(config_section + scene_id + seed + strategy_version)`；命中则复用。
- 生成后立即执行几何验证：节点 footprint 与 Tilemap 障碍不冲突，Portal anchor 对应的 Tile 可通行。
- 当前阶段输出基于 `tile_size/base_extent` 的占位 TilemapMeta，缓存键与图层细分待后续迭代。

## 8. 输出结构
`GeneratedWorld` 包括：
- `locations[]`：Scene/Interactive 节点，含 `id/parent/kind/meta/coord_local/coord_global/layer_index/tilemap_ref`。
- `edges[]`：连接信息与 Portal 锚点。
- `spawns[]`：资源节点容量与速率。
- `tilemaps[]`：Tilemap metadata（id、尺寸、层列表、缓存路径）。
- `logs[]`：调试统计（重试次数、软约束得分、缓存命中率）。

运行时只消费内存结构；需要写盘时由 `ExportModule` 负责。

## 9. 校验与测试
- **生成前置**：配置 Schema 校验、scene_capacity 与必需节点匹配。
- **生成后硬校验**：
  - 节点唯一性、父子关系正确、越界检测。
  - Portal 对等性、可达性（图搜索 + flood fill）。
  - 资源节点参数合法且位于可导航区域。
  - Tilemap 与节点 footprint 相容。
- 当前阶段实现：节点 ID 唯一性、布局坐标存在性、边引用与 Portal 配对合法性；其他校验将在后续里程碑补齐。
- **软约束报告**：如对称性偏差、密度超标，写入日志供分析。
- **测试策略**：
  - 单元测试：模块级 determinism（同 seed 输出 hash 相同）、约束违规触发错误。
  - 属性测试：随机种子下验证不变量（无孤立场景、总资源量在范围内）。
  - 金样对比：保留少量配置的 `world.json + tilemap` 基准，用于回归 diff。

## 10. 非目标（当前阶段）
- 流式 / 无限世界（按需拓展功能留给未来）。
- 编辑器驱动的手工摆放（与原则冲突）。
- 自动剧情/任务生成（仅预留元数据挂点）。
- 分布式/网络生成（当前关注单进程 determinism）。

## 11. 后续扩展
- 扩充规则库：生态、叙事事件、季节变化等。
- 导入外部噪声/地形资源并与现有策略混合。
- 增加多线程生成、分块缓存以支撑超大地图。
- 面向工具链的诊断可视化（离线渲染 Scene 布局快照）。

---

相关文档：[world/world-model.md](./world-model.md)（数据契约）、[world/world-representation.md](./world-representation.md)（渲染参考）、[interface/sandbox/sandbox-gui-tilemap-rendering.md](../interface/sandbox/sandbox-gui-tilemap-rendering.md)（Tilemap 消费）。
## 12. 运行时整合与 Sandbox GUI
- genesis::core::Engine 提供显式 loadWorldFromFile/loadWorldFromJsonString 接口并移除演示世界回退，exportWorldGraph 用于导出当前 LocationGraph。
- genesis::runtime::Runtime 将 generateWorldFromConfig 与 loadWorldFromFile、saveWorldToFile 解耦：生成阶段仅产出图和日志，可选写入 JSON，加载时再手动调用。
- Sandbox::Gui::RuntimeBridge 扩展 generateWorld/loadWorld/saveWorld，执行过程中暂停后台线程，加载成功后重建 WorldAtlas 并清理旧快照。
- Sandbox GUI “World Generation” 面板支持配置路径、输出文件、随机/指定种子，以及独立的加载/保存操作，Scene 视图以棋盘格占位 Tilemap 区域。
- 默认提供 data/worldgen/default.toml 与保存路径提示，方便快速生成、存档并在 CLI/GUI 中复用。
