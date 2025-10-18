# 世界生成（World Generation）

目标：以 MapConfig 为核心抽象，递归生成“分层节点图 + Tilemap（表/里）”的完整世界，同时满足可复现（seed）、可配置（模板）与可校验（契约）的要求。本文描述生成输出、配置结构、流程与验证策略。

## 输出契约
- **LocationGraph**：包含 Region/District/Area/Anchor/Interaction/Portal 节点、边与 Portal 元数据，字段符合 `world_model.md`。
- **Tilemap 资产**：每个 `mapId` 输出 `outsideView` 与 `insideView`（`.tmj` 或 `.tmb`），以及可选的二进制索引。
- **索引文件**：`world.json`（Graph）+ `tilemaps/index.json`（mapId → 资源路径、格式、子 map 列表）。
- **可复现性**：相同 `seed + MapConfig` 生成结果同构；锚点/Portal 坐标保持一致。
- **P0 规模**：单 Region（小镇）内若干 District（街区、野外边缘），数个 Area（酒馆、住宅、森林空地）。

## MapConfig 抽象
生成最小单元为 `MapConfig`，描述一张 map 及其子 map：
```json
{
  "id": "tavern_interior",
  "areaKind": "Interior",
  "template": "templates/interior/tavern.json",
  "anchors": [
    { "role": "entrance", "count": 1 },
    { "role": "seat", "count": 4 }
  ],
  "interactions": [
    { "type": "food", "template": "counter", "count": 1 }
  ],
  "portals": [
    { "toChild": "tavern_basement", "role": "stairs", "direction": "down" }
  ],
  "children": [
    { "config": "configs/maps/tavern_basement.json", "overrides": { "seedOffset": 3 } }
  ]
}
```
- `template` 决定 inside/outside Tilemap 模板、默认锚点/交互点布局、材质集。
- `anchors/interactions` 定义需要实例化的节点类型，可附带 Trait（如仅供 NPC 使用）。
- `portals` 声明与父子或兄弟 map 的连接规则，生成后写入 LocationGraph。
- `children` 为子 MapConfig，可直接嵌入或引用外部文件；生成器递归展开。

根配置 `root_map.json` 定义世界入口 Map（例如 “MainStreet”），生成器只需从 root 出发即可递归构建整张世界。

## 生成流程
1. **加载 Manifest**
   - 读取 `root_map.json` 与关联模板、词表、噪声参数。
   - 初始化 RNG（`seed` + `seedOffset`），封装成 `IRng` 便于测试。
2. **递归构建 map**
   - 依据 `template` 构造 inside/outside Tilemap：可拼装静态块或运行噪声算法。
   - 放置 Anchor/Interaction：根据模板候选点与配置数量，从 RNG 中挑选或生成坐标。
   - 创建 `LocationNode`：为每个锚点/交互点生成 nodeId，标记 `mapId`、`coord_local`、`role`。
   - 记录 Portal：父子之间双向创建 Portal 节点与边，写入 `PortalMeta`。
3. **汇聚 Graph**
   - 将 Area 节点插入父 District，District 插入 Region；自动生成父子边。
   - 生成 Area 之间的逻辑边：根据模板指定或基于空间关系自动推断（例如相邻房屋的街道 Portal）。
   - 对 Interaction 节点附加资源配置（容量、再生率、Trait 限制）。
4. **校验**
   - 局部：锚点坐标落在可通行 Tile，Portal 成对，交互点不重叠，子 map 存在。
   - 全局：树结构完整、图连通、nodeId 唯一、Portal 目标有效。
   - 扩展检查：资源覆盖率、Trait 分布、随机名重复等。
5. **落盘**
   - `LocationGraph` → `world.json`
   - Tilemap（inside/outside）写入 `data/tilemaps/generated/<mapId>.*`
   - 生成 `tilemaps/index.json`（mapId → inside/outside 路径、格式、子 map、Portal 列表）
   - 可选导出便于调试的快照（例如 SVG、截图、Markdown 摘要）。

## 模板系统
- 模板放置在 `data/world/templates/`，分层管理：`region/`、`district/`、`area/`、`tile/`。
- 每个模板包含：
  - `defaultAnchors`、`defaultInteractions`、`portalPatterns`
  - Tile 片段引用或噪声参数（高度图、对象刷子等）
  - Trait/需求约束（例如 “只有高 O 的 NPC 才会访问”）
- 可在 `MapConfig` 中覆写模板参数（尺寸、密度、随机候选数量）。

## 测试与验证
- **单元测试**
  - `MapConfig` 池：相同 seed 输出锚点/Portal 列表一致。
  - 校验函数：锚点在合法 Tile、Portal 成对、资源点配置完整。
- **集成测试**
  - 从 root 配置生成整图 → `WorldRegistry` + `TilemapRegistry` 加载 → 运行 100 步无错误。
  - GUI 烟雾测试：加载生成世界，验证 Map/Scene 视图能正确渲染与导航。
- **数据对比**
  - 保存小规模世界金样（graph + tilemaps），生成后与金样对比关键字段，差异需人工确认。
- **性能**
  - 记录生成时间、校验时间、Tilemap 打包耗时，确保在 CI 环境下也能运行。

## 里程碑（建议）
1. **P0**：支持 root MapConfig 递归生成、LocationGraph 输出、Tilemap 落盘与基本校验。
2. **P1**：模板系统与 Trait/资源配置，CLI/GUI 集成基本完成。
3. **P2**：噪声地形/Chunk 支持（与 `chunked_tile_graph.md` 对齐），Portal 拼接优化。
4. **P3**：二进制 Tilemap 格式、批量生成工具、自动化回归测试。

## 非目标（当前阶段）
- 大规模实时噪声地形（Procedural Infinite World），暂在专题文档讨论。
- 复杂剧情/任务线自动嵌入（保留接口，待未来系统接入）。
- 多世界并行实例（需 runtime 支持，当前仅默认单世界）。

## 相关文档
- 运行时世界模型：`world_model.md`
- Graph ↔ Tilemap 协同：`world_representation.md`
- 噪声/区块拓扑：`chunked_tile_graph.md`
