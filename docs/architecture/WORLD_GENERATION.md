# 世界生成（World Generation）

目标：以 **Scene 布局描述 + Interactive 数据** 为核心输出，构建可复现、可校验、可扩展的世界。生成器需要保证产出的 `world.json` 满足 `world_model.md` 定义的契约，并为渲染层提供足够的坐标与元信息。

## 1. 输出契约
- `world.json`
  - `locations[]`：Scene 与 Interactive 节点，包含 `id`、`parent`、`kind`、`navigable`、可选 `coord_local/coord_global`、`layout` 等字段。
  - `edges[]`：Scene 间连通性；Portal 需写入 `anchors.at_from/at_to` 以标记进出站位。
  - `spawns[]`：资源点配置（容量、产率、位置）。
  - `tilemaps[]`（可选）：Scene → Tilemap 基础信息（宽高、tileSize）。
- 资产文件（可选）：若 Scene 对应 Tilemap，导出 `.tmj/.tmb` 或自定义格式。
- 可复现性：相同 `seed + 配置` 生成的节点拓扑、坐标、资源布置应一致。

## 2. 配置抽象

推荐使用 `SceneConfig`（逻辑） + `LayoutTemplate`（几何）组合：

```json
{
  "id": "riverside",
  "scene": {
    "kind": "Area",
    "theme": "river_bank",
    "navigable": true
  },
  "layout": {
    "type": "grid",
    "origin": [100, 42],
    "spacing": [4, 2],
    "children": [
      { "ref": "scene:fishing_pier", "offset": [0, 0] },
      { "ref": "scene:market_stall", "offset": [6, 0] },
      { "ref": "interactive:portal:ferry", "offset": [8, 1] }
    ]
  },
  "children": [
    { "config": "configs/scenes/fishing_pier.json" },
    { "config": "configs/scenes/market_stall.json" }
  ]
}
```

- `scene`：语义描述（主题、可导航性、Trait 约束等）。
- `layout`：声明子节点摆放方式（支持 `grid`、`ring`、`noise`、`script` 等自定义类型）。
- `children`：引用子 Scene/Interactive 配置；生成器在递归时会分配 nodeId 并写入父子关系。

## 3. 生成流程
1. **加载 Manifest**：读取根 SceneConfig、模板、词表、噪声参数，初始化 RNG（`seed + seedOffset`）。
2. **分配 ID**：深度优先遍历配置树，按顺序分配 `LocationId`，建立临时父子关系表。
3. **布局解析**：
   - 根据 `layout` 为子节点写入 `coord_local`。
   - Scene 若指定 `origin` / `rotation` / `spacing`，同步计算 `coord_global`。
4. **生成 Interactive**：
   - `resource`：写入 `spawns[]`（容量、速率、local_coord）。
   - `portal`：生成对应的 `PathEdge`，并在 `anchors` 中填入进出锚点。
5. **写入边**：
   - Scene 之间的边来自配置或算法推断（网格邻接、河道对岸等）。
   - Portal 自动生成双向边（若配置 `bidirectional=true`）。
6. **校验**：
   - 节点：parent 必须存在、坐标在合法范围、ID 唯一。
   - Portal：成对存在、目标 Scene 可达、锚点落在可导航格子。
   - 资源：容量/速率为正数、关联的 Scene 可导航。
7. **落盘**：输出 `world.json`，并根据需要写入 Tilemap 与辅助索引。

## 4. 校验与测试
- **单元级**：
  - Layout 解析：同一个配置多次生成应得到相同的 `coord_local/coord_global`。
  - Portal/资源：缺失目标或坐标越界应在生成阶段抛出错误。
- **集成级**：
  - 将生成输出加载到 `WorldRegistry`，运行 Smoke 测试（资源消耗、Portal 传送）。
  - GUI 烟雾测试：渲染 Scene/Interactive 节点，确认锚点与布局可视化正确。
- **数据对比**：为关键样本保留金样（world.json + 关键 Tilemap），生成后 diff 关键字段。

## 5. 与运行时的协同
- 生成器写入的 `layout` / `coord_*` 信息直接被运行时与 GUI 消费；缺失字段虽可加载，但会降低可视化质量，应在生成时尽量补齐。
- 若 Scene 配置无法在生成阶段给出精确坐标（例如程序化噪声），需写入可复现参数（seed、尺寸），由运行时或工具按需再计算。
- 对于大地图，可在生成阶段拆分为多个 chunk Scene，每个 Scene 独立布局、独立写入 Tilemap 元信息。

## 6. 非目标（当前阶段）
- 无限/流式世界（需要分布式加载与惰性生成）。
- 自动剧情/任务植入（保留元数据接口，待后续系统接入）。
- 多实例并行（当前运行时专注单世界）。

---

相关文档：`world_model.md`（节点契约）、`world_representation.md`（渲染对齐）、`chunked_tile_graph.md`（大图探索笔记）。
