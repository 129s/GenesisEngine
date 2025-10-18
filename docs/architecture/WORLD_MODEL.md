# 世界模型（World Model）

GenesisEngine 的世界模型以 **Scene/Interactive 节点树** 为核心，辅以统一的整格坐标体系。运行层依靠该树驱动寻路、交互与资源调度；渲染层和工具仅消费这些数据，不回写逻辑状态。本章定义节点字段、坐标契约、布局描述以及运行时查询能力。

## 1. 节点树概览

```
Scene (root)
 ├─ Scene（子区域/子场景）
 │   └─ Interactive（resource / portal / ...）
 └─ Interactive
```

- **Scene**：容器节点，可嵌套；负责确定子节点布局、派生全局坐标。
- **Interactive**：叶节点，只承担交互。当前内置 `resource` 与 `portal` 两种类型，后续可扩展。

所有节点都由 `WorldRegistry` 按 `id` 管理，并通过 `childrenOf(id)` 暴露父子关系。

### 1.1 公共字段
| 字段 | 说明 |
| --- | --- |
| `id:uint32` | 全局唯一 ID。0 预留为 `InvalidLocation`。 |
| `parent:uint32` | 父 Scene 的 ID。根 Scene 取 0。 |
| `name:string` | 可读名称，供调试/渲染使用。 |
| `kind:LocationKind` | 语义标签（Region/Area/Room/Point）。运行时主要依赖 `navigable` 判定可走性。 |
| `navigable:bool` | 是否可作为寻路节点；大部分 Scene/Interactive 为 true，Portal 若仅作占位可设为 false。 |
| `coord_global:optional<[int,int]>` | 绝对整格坐标（世界空间）。Scene 在布局完成后应为子节点填写。 |

### 1.2 Scene 扩展字段
- `layout`（可选）：子节点布局描述（见第 3 章）。
- `metadata`：与生成或渲染相关的附加信息（主题、地形、种子等）。

### 1.3 Interactive 扩展字段
- `interactive.type`：`resource` / `portal` / …
- `coord_local:optional<[int,int]>`：相对于父 Scene 的整格坐标。
- 资源点：容量、产率等配置写入 `ResourceSpawn`，由 `ResourceSystem` 消费。
- Portal：目标 Scene、入口/出口锚点等写入 `PathEdge` 或 Portal 元信息（见 2.3）。

> **兼容性**：`WorldLoader` 对 `coord_global`、`coord_local`、`anchors` 等字段均为可选解析；旧数据缺失这些字段不会导致加载失败。

## 2. 坐标与连通性

### 2.1 坐标体系
- **全局坐标 (`coord_global`)**：以 Tile 为单位的整数坐标，供渲染层与调试工具直接使用。
- **局部坐标 (`coord_local`)**：Scene 内的相对整格坐标。Scene 在布局阶段根据自身原点与尺度换算成全局坐标。
- Scene 自身可以没有局部坐标（作为原点），但必须确保所有子节点能推导出 `coord_global`。

### 2.2 边（PathEdge）
`PathEdge` 表示 Scene/Interactive 之间的连通性，用于逻辑寻路。

| 字段 | 说明 |
| --- | --- |
| `from / to : LocationId` | 起点与终点节点。 |
| `cost:float` | 移动成本。 |
| `bidirectional:bool` | 是否自动生成反向边。 |
| `anchors.at_from / at_to : optional<[int,int]>` | 在起点/终点 Scene 局部坐标系中的锚点（Portal/入口）。 |
| `polyline` | （可选）供渲染/调试使用的路径折线。 |

Portal 会生成至少一条边：从入口 Scene 到出口 Scene。若 `bidirectional=true`，加载器会自动补齐反向边并交换锚点。

### 2.3 Portal 元信息
- Portal 本身是 Interactive 节点，携带局部坐标。
- 对应的 `PathEdge` 通过 `anchors` 提供进入/离开场景时的站位。
- 渲染层可读取 Portal 节点与 `anchors` 来绘制传送门、场景切换提示。

## 3. Scene 布局描述（Layout Descriptor）

Scene 负责为子节点分配局部坐标。布局描述存放在 Scene 的 `layout` 字段，推荐使用数据驱动配置，便于生成器与工具复用。

示例：
```json
{
  "id": 100,
  "name": "Riverside",
  "layout": {
    "type": "grid",
    "origin": [0, 0],
    "spacing": [4, 2],
    "children": {
      "scene:fishing_spot": { "offset": [2, 0] },
      "scene:pier": { "offset": [6, 0] },
      "interactive:portal:ferry": { "offset": [8, 1] }
    }
  }
}
```

常见模式：
- `grid`：规则网格，适合城镇/室内布局。
- `ring`：环形摆放，适合广场/篝火营地。
- `noise` / `script`：自定义算法或脚本生成，写入 deterministic seed。

Scene 在生成/加载阶段解析 `layout`，为每个子节点写回 `coord_local`，继而计算 `coord_global = scene.origin + transform(coord_local)`。

## 4. 运行时查询能力

`WorldRegistry` 提供以下只读接口：
- `findLocation(id)`：返回节点详情。
- `childrenOf(id)`：查询子节点列表。
- `edgesFrom(id)`：获取以节点为起点的边集。
- `spawnsAt(id)` / `resourceSpawns()`：资源点枚举。
- `tilemaps()`：返回与节点关联的 Tilemap 元数据（若有）。

该集合支撑 MovementSystem、Planner、渲染层等模块。所有写操作仅在加载/生成阶段或受控 API 中进行。

## 5. 渲染与工具消费
- 渲染层通过 `coord_global` 与 `Interactive.type` 生成可视标记，只绘制视口内数据，实现逻辑与渲染的近似同步。
- GUI 若需要 Tilemap 细节，可结合 `TilemapMeta`（尺寸、tileSize）与 Scene 布局进行投影。
- 调试工具可以读取 `layout` 描述，复现生成时的摆放决策，或为编辑器提供可视化。

## 6. 与世界生成的关系
- 世界生成器负责产出满足上述契约的 `world.json`。
- Scene 的 `layout`、Portal `anchors`、资源配置等应由生成器或手工数据在产出阶段写入。
- 运行时加载器不再强制这些字段存在，但若缺失，渲染层无法获得位置参考，应在调试日志中提示。

---

相关文档：`world_generation.md`（数据生产流程）、`world_representation.md`（运行层与渲染层对齐）、`sandbox_gui_tilemap_rendering.md`（GUI 渲染契约）。
