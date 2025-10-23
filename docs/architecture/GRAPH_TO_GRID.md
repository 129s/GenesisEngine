# 图到矩阵（Graph → Grid）映射说明（历史资料）

> 2025-10-23 起 `sandbox_cli` 模块已移除，本文保留作为 ASCII 渲染方案的历史记录，便于回顾早期实现与后续若需复刻时参考；当前主力前端为 GUI。

## 核心结论
- 模拟内部维护“分层节点图（`LocationGraph`）”，CLI（已退场）当时仅仅读其 `nodeId`。
- CLI 不做几何布局计算，而是使用“布局映射（Layout）”把 `nodeId → (x,y)` 投影到字符网格。
- 渲染使用运行时快照（Telemetry）中的地点标识（`LocationId`），根据布局坐标在网格上“盖章（stamp）”字符。

## 数据路径（历史）
- 世界图定义仍位于 `include/genesis/world/WorldTypes.hpp` — `struct LocationGraph { nodes, edges, spawns }`
- 运行时抓取快照：`src/core/Engine.cpp` 中的 `captureTelemetry` 汇总每步数据，产出：
  - 资源：`Telemetry::ResourceSnapshot`（含 `location`）
  - 行动：`Telemetry::ActionSnapshot`（含 `target`）
  - 代理：`Telemetry::AgentSnapshot`（含 `location`）
- CLI 渲染输入（历史路径）：`include/sandbox/CliRenderer.hpp` — `render(const TickTelemetry&)`
- 布局映射（历史类型）：`Layout{ width,height, nodes: map<locationId, LayoutNode{x,y,label}> }`
- 渲染逻辑（历史实现）：`src/sandbox/CliRenderer.cpp` 根据快照中的 `LocationId.value` 查找布局坐标，并在 `grid[y][x]` 处写符号。

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
  - 资源：Food → `'F'`，其他 → `'R'`（历史实现位于 `src/sandbox/CliRenderer.cpp`）
  - 行动：`ConsumeResource` → `'C'`，其他行动 → `'M'`
  - 代理（Agent）→ `'A'`
- 目前不绘制边（`edges`）或路径，仅对节点进行投影；后续可扩展在字符网格描边连接（选做）。

## 布局来源与定制
- 默认布局：CLI 时代通过 `defaultLayout()` 与 `data/ascii_layout.json` 对齐旧 Demo 世界；相关文件现已移除。
- 外部文件：曾可通过 `--layout <path>` 指定 JSON，格式可参考仓库历史中的 `docs/guides/sandbox-cli.md`。
- 关键约束：布局中的 `id` 必须与世界图中 `LocationNode.id.value` 一致，才能正确投影。

## 设计取舍
- 解耦：图拓扑与 ASCII 布局相互独立，便于 GUI/其他前端共用数据。
- 简洁：CLI 不做自动布局或路径渲染，复杂可视化交由 GUI 完成。
- 遗留维护：若未来需要复刻 CLI，可参考本档案恢复布局文件并保持 `nodeId` 对齐。

## 快速自检（历史）
- 现象：某节点在 CLI 不显示 → 检查 `layout.nodes` 是否包含对应 `id`，以及坐标不越界。
- 现象：同一格出现冲突 `'#'` → 该坐标同时被资源/行动/代理盖章，调整布局坐标或渲染优先级即可。
- 现象：世界加载正常但布局错误 → 确保布局 JSON 中的 `id` 与世界数据同步。
