# 图到矩阵（Graph → Grid）映射说明（CLI 遗留）

> 本文仅用于维护 `sandbox_cli` 的 ASCII 可视化。当前主力前端为 GUI；CLI 仅在过渡期或排障时使用。

## 核心结论
- 模拟内部维护“分层节点图（`LocationGraph`）”，CLI 只读其 `nodeId`。
- CLI 不做几何布局计算，而是使用“布局映射（Layout）”把 `nodeId → (x,y)` 投影到字符网格。
- 渲染使用运行时快照（Telemetry）中的地点标识（`LocationId`），根据布局坐标在网格上“盖章（stamp）”字符。

## 数据路径（从图到 CLI）
- 世界图定义：`include/genesis/world/WorldTypes.hpp:61` — `struct LocationGraph { nodes, edges, spawns }`
- 运行时抓取快照：`src/core/Engine.cpp:120` 起的 `captureTelemetry` 汇总每步数据，产出：
  - 资源：`Telemetry::ResourceSnapshot`（含 `location`）
  - 行动：`Telemetry::ActionSnapshot`（含 `target`）
  - 代理：`Telemetry::AgentSnapshot`（含 `location`）
- CLI 渲染输入：`include/sandbox/CliRenderer.hpp:27` — `render(const TickTelemetry&)`
- 布局映射：`include/sandbox/CliRenderer.hpp:16` — `Layout{ width,height, nodes: map<locationId, LayoutNode{x,y,label}> }`
- 渲染逻辑：`src/sandbox/CliRenderer.cpp:34` — 根据快照中的 `LocationId.value` 在 `Layout.nodes` 中查坐标，并在 `grid[y][x]` 处写符号。

## 渲染细节
- 网格初始化：`width × height`，默认填充 `'.'`。
- 位置盖章（伪码）：
  1. 查快照对象的 `LocationId`/`target`；
  2. 用 `id` 在 `Layout.nodes` 找到 `(x,y)`；
  3. 调用 `stamp(grid,x,y,symbol)` 写入字符：
     - 空位 `'.'` → 写入符号
     - 非空且与符号相同 → 保持
     - 冲突（不同符号重叠）→ 写入 `'#'`
- 符号约定（当前实现）：
  - 资源：Food → `'F'`，其他 → `'R'`（`src/sandbox/CliRenderer.cpp:52`）
  - 行动：`ConsumeResource` → `'C'`，其他行动 → `'M'`（`src/sandbox/CliRenderer.cpp:58`）
  - 代理（Agent）→ `'A'`（`src/sandbox/CliRenderer.cpp:63`）
- 目前不绘制边（`edges`）或路径，仅对节点进行投影；后续可扩展在字符网格描边连接（选做）。

## 布局来源与定制
- 默认布局：`src/apps/sandbox_cli/Main.cpp:126` 的 `defaultLayout()` 与 `data/ascii_layout.json` 保持一致（仅覆盖旧 Demo 世界）。
- 外部文件：通过 `--layout <path>` 指定 JSON，格式见 `docs/guides/sandbox-cli.md` 的“Layout configuration”。
- 关键约束：布局中的 `id` 必须与世界图中 `LocationNode.id.value` 一致，才能正确投影。

## 设计取舍
- 解耦：图拓扑与 ASCII 布局相互独立，便于 GUI/其他前端共用数据。
- 简洁：CLI 不做自动布局或路径渲染，复杂可视化交由 GUI 完成。
- 遗留维护：仅在 CLI 仍被使用的场景下更新布局文件；如世界模型演进，请确保 `ascii_layout.json` 与最新 `nodeId` 对齐，否则 CLI 渲染会错误。

## 快速自检（Troubleshooting）
- 现象：某节点在 CLI 不显示 → 检查 `layout.nodes` 是否包含对应 `id`，以及坐标不越界。
- 现象：同一格出现冲突 `'#'` → 该坐标同时被资源/行动/代理盖章，调整布局坐标或渲染优先级即可。
- 现象：世界加载正常但布局错误 → 确保 `data/world/*.json` 中 `id` 与 `ascii_layout.json` 同步。
