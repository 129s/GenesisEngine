# 世界生成架构（World Generation）

目标：构建“Map 图 + Scene 分组 + Interaction/Portal 填充”的程序化世界生成流水线，产出满足 `world/world-model.md` 的数据产物；运行时仅依赖这些产物，不依赖 Tilemap/碰撞。生成必须可复现（`seed + 配置`），无需手工摆放。

## 核心原则
- 最小权威数据：Map 列表、Map 有向边、每图的 Scene/Interaction/Portal 集合与坐标。
- 渲染解耦：Tilemap 属于表现资产，是否生成由配置决定；运行时不读取 Tilemap/碰撞。
- 模块化与确定性：拓扑（MapGraph）→ 布局（Scene）→ 交互填充（Interaction/Portal/Resource）→ 规则化连边（MapEdge）→ 验证 → 导出。
- 可缓存：重资产（如 Tilemap）可缓存；但世界结构本身应由代码可重建。

## 1. 库边界与整合
- 生成库 `genesis_worldgen`（已存在）保持独立目标；仅依赖 STL、配置解析、RNG；不依赖渲染/运行时。
- 对外 API（保持风格，调整语义）：
  - `GeneratorConfig load_config(path)`
  - `GeneratedWorld generate_world(const GeneratorConfig&, Seed)`
  - `bool write_world_artifacts(const GeneratedWorld&, paths)`（可选：写 world.json 与 map_{id}.json 以及表现资产）

## 2. 模块划分（对齐新模型）

| 模块 | 职责 | 说明 |
| ---- | ---- | ---- |
| `MapGraphModule` | 生成 Map 列表与候选 Portal 频道、规则 | 决定 Map 数量、标签与初始规则集 |
| `SceneLayoutModule` | 构建每张 Map 的 Scene 树与布局 | 仅分组与坐标继承，不产生命名导航节点 |
| `InteractionModule` | 在各 Scene 下放置交互点 | Resource/Portal 等，写入局部/全局坐标 |
| `PortalEdgeModule` | 从 Portal 集合导出 MapEdge | 按规则/白名单/频道/邻近生成有向边与代价 |
| `ValidationModule` | 一致性与规则校验 | ID 唯一、坐标存在、Portal 合法、MapEdge 连通性 |
| `ExportModule` | 导出世界产物 | world.json（maps/map_edges）、map_{id}.json（scenes/interactions/portals/...）|
| `TilemapModule`（可选） | 生成渲染用 Tilemap | 只输出表现资产与 metadata，不参与运行时 |

各模块通过 `WorldGenContext` 共享 RNG/日志/配置与缓存接口。

## 3. 数据流
1) 配置加载：读 TOML/JSON → `GeneratorConfig`（世界规模、主题、频道/白名单规则、布局策略）。
2) MapGraph：产出 `maps[]`，并准备 Portal 规则（频道/白名单/方向/基础费）。
3) Scene 布局：每张 Map 内构造 Scene 树，分配子 Scene 的几何位置/原点/变换；仅用于坐标继承。
4) 交互填充：在 Scene 下放置 `interactions[]`（Resource/Portal/...），写 `coord_local` 并可选计算 `coord_global`。
5) MapEdge 生成：基于 Portal 集合与规则生成 `map_edges[]`（有向，附 `cost` 与可选 `rules`）。
6) 验证：ID 唯一、坐标存在、引用合法、MapEdge 连通性；（不再做 Tilemap 碰撞/导航校验）。
7) 导出：写 `world.json` 与每张 `map_{id}.json`；可附带调试日志与统计。
8) （可选）Tilemap：按 Scene/主题生成/复用 Tilemap 资产与 metadata，但不参与校验与运行时。

## 4. 配置结构（示例）

```toml
[world]
seed_offset = 0
theme = "frontier_outpost"

[maps]
count = 2
names = ["Town", "Dungeon"]

[portal_rules]
channel = ["door", "stairs", "gate"]
; white_list = [{ from_map=1, to_map=2, channel="stairs" }]
default_cost = 1.0

[layout.grid]
cell_width = 5.0
cell_height = 4.0
columns = 3
margin = 1.0

[interaction.resources]
food = { density = 0.2, capacity = 24, rate = 3 }
social = { density = 0.1, capacity = 12, rate = 2 }
```

## 5. 约束模型（更新）
- 世界层：`maps` 非空、ID 唯一；`map_edges` 有向引用合法，无自环（或允许自环需注明语义）。
- Map 内：`scenes` 父子引用合法；`interactions` 必有 `sceneId` 与坐标；`portal.interactionId` 合法。
- Portal 规则：若使用频道/白名单/方向，生成的 `map_edges` 必符合规则；代价为非负。
- 非目标：不校验 Tilemap 碰撞/可达性（运行时不消费）。

## 6. Tilemap 集成（仅表现层，可选）
- 是否生成 Tilemap 由配置决定；生成的资产仅用于渲染与编辑器预览。
- 输出 `TilemapMeta{ nodeId/sceneId, width,height,tileW,tileH }` 等元数据，便于前端尺寸对齐。
- 不生成/不校验碰撞/导航掩码；一切阻断/通行语义均由 Map 划分与 Portal 连接表达。

## 7. 输出结构
- 世界总表（`world.json`）：
  - `maps[]: { id,name,meta? }`
  - `map_edges[]: { from,to,cost,rules? }`
- 每图分表（`map_{id}.json`）：
  - `map: { id,name }`
  - `scenes[]: { id,parent?,name,origin?,transform?,meta? }`
  - `interactions[]: { id,sceneId,kind,coord_local,[coord_global]?,meta? }`
  - `portals[]: { interactionId,channelId?,oneWay?,teleportCost? }`
  - `tilemap?: { width,height,tileW,tileH }`
- 诊断：`logs[]`（生成统计/软约束分数/缓存命中率）。

## 8. 校验与测试
- 配置 Schema 校验、ID 唯一性、引用合法性、MapEdge 连通性。
- 软约束：对称性/密度/主题贴合度评分，仅记录日志。
- 测试策略：
  - 确定性：同配置与种子输出哈希一致。
  - 属性测试：随机种子验证不变量（无悬空引用、Map 图强连通或符合规则）。
  - 基准样本：保留 `world.json + map_{id}.json` 少量金样用于回归。

## 9. 非目标（当前阶段）
- 流式/无限世界生成。
- 手工摆放编辑工作流。
- 自动剧情/任务生成（仅预留元数据挂点）。
- 分布式/网络生成。

## 10. 后续扩展
- 规则库：生态/叙事事件/季节变化。
- 资产：引入多主题 Tile 集并生成不同外观版本。
- 性能：多线程与分块生成、增量写盘。
- 工具：离线可视化导出（Scene/Interaction 快照）。

——
相关文档：`world/world-model.md`（数据契约）、`world/world-representation.md`（渲染参考）、`../interface/sandbox/sandbox-gui-tilemap-rendering.md`（Tilemap 消费）。

## 11. 运行时整合与 Sandbox GUI（对齐新模型）
- 运行时加载：先加载 `world.json`（Map 图）再按需加载各 `map_{id}.json`（Scene/Interaction/Portal/TilemapMeta）。
- Atlas 重建：WorldAtlas 聚合 Map 与每图的节点/交互点坐标；仅用于可视与查询，不影响运行时语义。
- GUI 面板：
  - World Generation：生成→落盘→加载的分步按钮；可选生成 Tilemap。
  - Map 选择：切换当前 Map 的可视；Portal/MapEdge 可视化与过滤（频道/白名单）。
  - Inspector：交互点为一等公民（可选中/定位），Scene 仅用于分组浏览。

