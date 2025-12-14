# MVP：Scene/Interactive 节点树重构

> 状态：历史资料。该 MVP 基于 v1 `WorldRegistry/LocationGraph` 时代的节点/锚点设想；当前主线世界契约已切换为 v2 `WorldDatabase`（Map/Scene/Interaction/Portal）与 `world.json + map_{id}.json` 目录结构。请优先参考：
> - `docs/architecture/world/world-model.md`
> - `docs/handbook/WORLD_SCHEMA_MIGRATION.md`

## TL;DR
- 统一运行层 Scene/Interactive Node 树与渲染坐标基线，让交互节点、Portal 锚点与资源分布拥有单一数据源。
- MVP 交付聚焦资源采集与跨场景传送闭环，为环世界/矮人要塞式玩法的程序化扩展奠定架构。
- 完全取消“编辑器 + 预制”假设，所有内容通过生成器/组合机制产出，并淘汰原型阶段的旧数据与实现。

## 背景
- 运行层缺乏稳定的 Scene/Interactive 抽象，节点定位、传送锚点和资源点散落在多套系统中，维护成本高。
- 渲染与工具链需要统一的全局/局部坐标信息以完成视口裁剪、交互提示和 Tilemap 对齐。
- 原型阶段的生成流程对坐标与布局的描述能力有限，难以驱动大规模程序化内容和组合式玩法。

## 目标
1. 建立针对 `scene`/`interactive` 的统一 Node 运行时模型，并完成旧逻辑迁移。
2. 在 Loader/WorldRegistry 中贯通全局与局部坐标，Portal 锚点、资源节点等运行数据可查询。
3. 让渲染层与工具层读取同一份数据完成可视化（裁剪、锚点调试、Tilemap 对齐）。
4. 全面替换原型期的实现与数据，统一到新 Schema 与生成流水线，不再维护兼容层。

## 非目标
- 不提供也不规划可视化编辑器界面，调试仅依赖程式化工具与日志。
- 不重新定义资源系统行为，仅补充定位相关字段。
- 不处理 AI Planner 的策略升级，只保证在新 Node 接口下保持原有能力。

## 成功指标
- 运行层加载旧/新 JSON 世界均成功，并能在 Node 树中查询到新增字段。
- NPC 能够依赖新 Node 接口完成跨 scene 寻路、到达资源点并产生库存/饥饿变化。
- Portal 锚点在日志或调试渲染中可视化，跨 scene 切换稳定。
- 大图（≥4k 节点）寻路性能无明显回退，渲染层仅绘制视口内节点。
- Loader、Movement、Resource、Planner、Runtime、渲染调试相关测试全部通过。

## 工作范围
### 运行层
- Node 树包含 `scene`（可嵌套子节点）与 `interactive`（叶子，含 `portal`、`resource` 等）两类。
- 节点具备全局/局部整格坐标，生成或加载时写入。
- `scene` 通过数据驱动的布局描述（JSON）定义子节点位置，Loader 构建 Node 树时回填坐标。
- 沿用 `ResourceSystem`，补齐 Portal 入口/出口锚点、资源点容量/速率、交互站位等运行数据。
- 禁止手工预制运行层结构，所有初始节点由生成管线或组合器实时构建。

### 渲染层
- 利用节点坐标完成视口裁剪与图层对齐，避免渲染无关节点。
- Portal 锚点、交互热点以调试图元展示，支持快速验证。
- 为未来 Tilemap/高度层集成保留接口，不在 MVP 内强制实现。

### 世界生成 / 数据入口
- 更新 JSON Schema 以支持布局策略、局部坐标等描述。
- 扩展主生成管线，使场景拓扑、资源点与 Portal 完全由程序化规则组合产出，保留极少量种子配置。
- 废弃旧版静态数据文件，通过生成器重新导出所需测试世界，并在日志中标示缺失配置用以调试。

## 架构关键决策
- **节点模型**：Node 基类统一 id、parent、kind、meta；`scene` 维护有序子节点集合；`interactive` 针对 `portal`/`resource` 提供类型安全 payload。
- **坐标系统**：局部坐标由父 scene 的布局策略定义；全局坐标在 Loader 阶段递归计算（父全局 + 局部偏移）。
- **Portal 协议**：`portal` 记录 `anchors.at_from/at_to`；WorldRegistry 暴露查询接口供寻路与渲染使用。
- **资源交互**：资源节点依旧对接现有库存/消耗事件，新增 `local_coord` 辅助站位提示。
- **扩展预留**：Tilemap、导航网格、可编辑属性保持可选，避免阻塞 MVP 发布。

## 数据契约（MVP Schema）
- `locations[]`
  - `id, parent, name, kind(Point/Area/Building/Region), navigable`
  - 可选：`coord_global:[x,y]`（渲染参考）
  - 可选：`coord_local:[x,y]`（父 scene 内相对位置）
  - 可选：`layout`（scene 专用：布局策略名 + 参数）
- `edges[]`
  - `from, to, cost, bidirectional`
  - 可选：`anchors{ at_from:[x,y], at_to:[x,y] }`（场景局部锚点/Portal 锚点）
- `spawns[]`（resource）
  - `name, type(Food/Drink/Social), location, capacity, rate_per_step`
  - 可选：`local_coord:[x,y]`（交互点站位）
- `tilemaps[]`（可选）
  - `node, width, height, tileSize`

迁移策略：旧版 JSON 将被弃用，新的生成器必须为上述字段提供完整数据；个别可选字段仅用于未来扩展，不作为兼容兜底。

## 迭代切分
### 阶段 0：准备与清理原型（已完成）
- 清点旧版 Scene/Interactive 原型实现，移除阻碍后续重构的依赖与入口。
- 保持 CLI 目标、Runtime 基线测试通过，确保生成器可重新导出当前示例世界。

### 阶段 1：Node 树与布局骨架
- 定义 Node/Scene/Interactive 运行时结构与接口，迁移现有交互逻辑。
- Loader 支持 scene 布局描述解析，递归回填 `coord_local` 与 `coord_global`。
- WorldRegistry 新增 portal 锚点与资源节点查询 API。
- 针对 Node 构建与坐标计算补充单元测试。

### 阶段 2：交互闭环与寻路验证
- NPC 移动/Planner 改用新 Node 接口，验证跨 scene 寻路。
- 资源点库存变更、饥饿下降快照纳入集成测试。
- Portal 切换流程读取 `anchors` 并输出调试日志。
- 补充冒烟场景：嵌套 scene、大图（≥4k 节点）运行基准。

### 阶段 3：渲染与工具整合
- GUI Map 视图读取全局/局部坐标完成视口裁剪与节点放置。
- Portal/Resource 调试图标展示锚点与站位。
- Tilemap 信息读取与占位渲染，缺失时提供 UI 提示。
- 若自动化渲染回归暂缺，则补充手动验证脚本或记录步骤。

## 验收标准
1. 运行层能够加载旧/新 JSON 世界，新字段在 Node 树中可查询。
2. NPC 可以到资源点完成消耗流程，日志出现“库存下降 + 饥饿降低”快照。
3. Portal 锚点在日志或调试渲染中可视化，跨 scene 切换稳定。
4. 大图（≥4k 节点）寻路与运行时性能保持同阶，渲染仅绘制视口内节点。
5. Loader、Movement、Resource、Planner、Runtime、渲染调试相关测试全部通过。

## 测试策略
- **Loader**：缺失/非法坐标字段不导致失败；存在时正确回填坐标与锚点。
- **Runtime**：嵌套场景与大图中 `agent` 可达资源点并完成交互；Portal 切换稳定。
- **CLI/GUI**：读取 `tilemaps[]` 元信息不影响运行；缺失时 UI 使用占位提示并保持交互能力。

## 风险与应对
- **生成覆盖不足**：通过 Schema 校验与调试输出锁定生成器缺漏，优先补齐规则而非引入手写数据。
- **可达性问题**：生成策略优先放置资源于可达节点，补充寻路诊断日志。
- **渲染坐标偏移**：在渲染整合阶段增加可视化断言，防止坐标不一致。
- **技术债累积**：阶段交付时同步代码文档与示例场景，降低后续迭代成本。

## 后续扩展方向
- 拓展程序化生成规则库（地形、结构原型、叙事事件）以提升组合涌现。
- 引入导航网格与细粒度寻路调优。
- 更丰富的渲染图层与 Tilemap 资源管理。
