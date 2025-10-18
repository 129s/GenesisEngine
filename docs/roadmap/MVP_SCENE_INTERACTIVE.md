# MVP：Scene/Interactive 节点树重构

目标：以“运行层 Node 树 + 渲染层参考坐标”的统一抽象，完成最小可用版本（MVP），支撑环世界/矮人要塞类玩法的基础交互（资源、传送/切换场景）。

## 范围（Scope）
- 运行层：
  - Node 树：`scene`（可有子节点）与 `interactive`（叶，类型含 `portal`、`resource`）。
  - 坐标：全局/局部整格坐标（可选），用于布局与渲染参考。
  - scene 具备“子节点布局描述”能力（数据驱动），生成/装载时确定子节点坐标。
  - 交互：
    - `resource`：库存、产率、消耗事件（沿用现有 `ResourceSystem`）。
    - `portal`：跨 scene/地图切换，包含入口/出口锚点坐标（局部）。
- 渲染层：
  - 基于视口裁剪，仅渲染可见节点。
  - 读取 Node 的全局/局部坐标作为放置参考；后续与 Tilemap 对齐。
- 世界生成：
  - scene 可声明子节点布局策略，写入 JSON；运行层据此填充 `coord_local`/`coord_global`。

## 数据契约（MVP Schema）
- `locations[]`：
  - `id, parent, name, kind(Point/Area/Building/Region), navigable`
  - 可选：`coord_global:[x,y]`（渲染参考）
  - 可选：`coord_local:[x,y]`（在父 scene 内的相对位置）
- `edges[]`：
  - `from, to, cost, bidirectional`
  - 可选：`anchors{ at_from:[x,y], at_to:[x,y] }`（场景局部锚点/Portal 锚点）
- `spawns[]`（resource）：
  - `name, type(Food/Drink/Social), location, capacity, rate_per_step`
  - 可选：`local_coord:[x,y]`（交互点站位）
- `tilemaps[]`（可选）：`{ node, width, height, tileSize }`

注：为兼容旧数据，`coord_global`/`anchors`/`local_coord` 在加载时一律为可选；渲染/工具链可提示缺失但不阻塞运行。

## 验收标准（Acceptance Criteria）
1) 运行时加载：
   - 能加载包含上述可选字段的世界 JSON；旧格式（不含坐标/锚点）同样成功。
2) 交互闭环：
   - NPC 在含资源点的世界中，能够通过规划/移动/消耗，出现“库存下降 + 饥饿降低”的快照。
3) 传送/切换：
   - Portal 的 `anchors` 在渲染层可被读取（先以调试信息/日志验证）。
4) 性能：
   - 大图（≥ 4k 节点）下，NPC 图级寻路与运行层计算耗时同阶；渲染仅绘制视口内信息。
5) 测试：
   - 单元测试全部通过；包含 Loader、Movement、Resource、Planner、Runtime 烟测。

## 里程碑（Milestones）
- M0 兼容旧数据：
  - 放宽 Loader 对 `coord_global`/`anchors`/`local_coord` 的强制要求（完成）。
  - CLI 目标链接引擎库，修复符号缺失（完成）。
  - Runtime 缺省日志器初始化（完成）。
  - 噪声世界生成 + Runtime 测试整体通过（完成）。
- M1 Scene 布局：
  - 为 `scene` 引入“子节点布局描述”解析与坐标回填（数据驱动）。
  - 将 Portal 锚点纳入 `WorldRegistry` 查询，渲染侧读取并绘制参考标记。
- M2 渲染侧对齐：
  - GUI Map 视图读取全局/局部坐标，视口裁剪，叠加 NPC 插值移动。

## 回归测试（Regression）
- Loader：
  - 缺少/非法 `coord_global`/`anchors`/`local_coord` 不再导致失败；解析存在时正确回填。
- Runtime：
  -嵌套场景/大图：`agent` 能找至资源点并消耗，出现库存下降与饥饿降低快照。
- CLI/GUI：
  - 读取 `tilemaps[]` 元信息不影响运行；当未提供时 UI 以占位呈现。

## 风险与对策
- 数据变体：允许可选字段，工具链提示而非阻塞；提供迁移脚本（后续）。
- 可达性：Spawn 点选择优先放在可达资源附近，避免“孤岛”导致长期无消费（已在引擎策略修正）。

