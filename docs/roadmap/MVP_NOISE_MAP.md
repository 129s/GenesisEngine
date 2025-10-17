# MVP 规划：两种地形的随机地图 + 固定刷新资源点 + NPC 基本生命周期

## 目标（Scope）
- 世界生成：基于噪声生成的地图，包含 2 种地形（例如 `Stone` 与 `Soil`）。
- 资源点：按固定时间步刷新（已由 `ratePerStep` 支持），分布受地形约束。
- NPC 生命周期：需求衰减→规划→移动→消耗资源→恢复需求 的最小闭环。

## 架构取舍
- 图作为元结构：先不引入 Tile 层寻路，使用 `LocationGraph` 抽象“区域/单元格”。
- 简化建模：将噪声阈值后的连通分量或网格单元映射为 `LocationNode`；用 4 邻接/8 邻接生成 `PathEdge`。
- CLI 展示：用现有 Layout（id→x,y）渲染。对规则网格可自动生成布局 JSON（行列→x,y）。

## 交付物（Deliverables）
1. 世界生成（P0）
   - `NoiseGridGenerator`：输入 `seed,width,height,threshold`，输出 `LocationGraph{nodes,edges,spawns}`。
   - 节点：每个可通行单元格（或小块）→ `LocationNode{ id,parent=Region, name, kind=Point }`；附 `terrain` 标签（字符串或枚举，先存在 name/tag）。
   - 连通：相邻可通行格创建 `PathEdge{cost=1.0,bidirectional=true}`。
   - 资源：在 `Soil` 上按密度采样 `ResourceSpawn{ Food, capacity, ratePerStep }`。
   - 输出：写入 `data/world/generated/noise_mvp.json` 与匹配的 `data/ascii_layout.json`（自动生成）。
2. 运行时接入
   - Engine 在找不到 `demo_world.json` 时，优先加载 `generated/noise_mvp.json`（或提供脚本/参数切换）。
   - 资源系统：沿用当前 `ratePerStep` 增产逻辑（固定刷新）。
3. NPC 基本循环
   - 维持现有 Need/Hunger/Planner/Action 流程；将代理初始落点设置在可通行节点上。
   - 验证代理可在图上移动到 Food 资源点并消费，需求回落后再次进入循环。
4. CLI 支持
   - 使用生成的布局 JSON（按网格行列生成 x,y），在 ASCII 网格观察 `A/M/C/F` 分布与变化。

## 验收标准（Acceptance Criteria）
- 生成：相同 seed → 相同 `noise_mvp.json`；不同 seed → 不同拓扑。
- 地形：统计占比近似阈值预期（例如阈值 0.5 时两类大致接近 1:1）。
- 资源：Food 刷新稳定，`current` 单调趋近 `capacity`，消费时下降。
- NPC：在若干步内从初始点移动到最近 Food 点，执行 `ConsumeResource`，Hunger 值降低；随后继续循环。
- CLI：`render` 能显示 `F`（资源）、`M/C`（移动/消费）、`A`（代理），步进时可见状态变化。

## 实施步骤（Milestones）
1) 生成器与布局脚本（或内置）
- 噪声：`std::mt19937` + 简单 hash/noise（或开源简单值噪声）→ 二值地形。
- 网格→图：遍历网格建立节点/边；按规则采样资源点。
- 网格→布局：`(col,row)` → `(x=col*2+offset, y=row+offset)` 生成布局 JSON。

2) 数据接入
- 在 `scripts/` 增加生成命令（PowerShell/Python）产出 world+layout。
- Engine 加载该 world；或 CLI 接受 `--layout` 指向生成布局。

3) 行为验证
- 启动 CLI，固定脚本命令：`step 5; pause; resume; step 30; quit`。
- 观察 Telemetry：`actions` 出现 `MoveTo/ConsumeResource`，`needs` 中 Hunger 波动符合预期。

## 非目标（本轮不做）
- 真正 Tile 层路径搜索/碰撞；液体/危险材质；多种资源/职业；复杂 AI；持久化/回放。

## 后续演进钩子
- 将 `terrain` 正式化为 `LocationNode` 扩展字段或标签集合（Loader/JSON 同步）。
- 引入 `Area/Portal` 与 Tile 层（参考 `CHUNKED_TILE_GRAPH.md` 与 `TILEMAP_INTEGRATION.md`）。
- 扩展资源类型（Drink/Social）与地形约束的资源分布规则。

## 执行计划（Action Plan）
- **Phase 1 · 生成器基础**（已完成）：实现 `NoiseGridGenerator`，构建 `LocationGraph`、资源采样与 JSON 导出，并补齐单元测试。
- **Phase 2 · 命令与自动化**：提供脚本/命令行一键生成噪声地图，接入构建流程并更新操作指南。
- **Phase 3 · 运行时接入**：引擎加载生成地图、维持资源刷新，设置代理初始落点并做集成测试。
- **Phase 4 · NPC 生命周期验证**：在 `sandbox_cli` 中使用固定脚本复现 `MoveTo → ConsumeResource` 闭环，记录 Telemetry。
- **Phase 5 · CLI 可视化支持**：渲染 ASCII 布局、补充验证指引。详见 `docs/status/backlog/noise-map-mvp.md`。
