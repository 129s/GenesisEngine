# 世界生成（World Generation）设计方案

> 目标：以可扩展、可复现（可通过 Seed 重现）的方式，生成当前引擎所采用的“分层地点图（LocationGraph）+ 资源点（ResourceSpawn）”世界数据，并可序列化为现有 JSON 格式，直接由 `WorldLoader` 加载或在运行时注入 `WorldRegistry`。

## 范围与输出契约
- 输出结构：`LocationGraph{ nodes, edges, spawns }`，与 `include/genesis/world/WorldTypes.hpp` 一致。
- 数据落盘：JSON 格式，字段同 `data/world/demo_world.json`（`WorldLoader` 已支持）。
- 可复现性：相同 `seed + GeneratorConfig` 生成一致结果。
- 规模目标（阶段性）：
  - P0：单一区域（镇/营地）数十个地点/若干建筑、基础资源点。
  - P1：多区域/分区（Residential/Market/Industrial），若干模板+规则。
  - P2：更复杂拓扑（路网/门禁/条件边）、动态地形挂钩（可选）。

## 生成管线（P0 MVP）
1. 种子与配置
   - 输入：`seed: uint64`、`GeneratorConfig`（建筑数量范围、房间最小/最大数、资源分配系数等）。
   - PRNG：`pcg32` 或 `std::mt19937`，封装为 `IRng` 便于测试。
2. 区域与建筑骨架
   - 固定 1 个 `Region` 根节点（如 “Town Center”）。
   - 建筑数目在范围内采样；提供若干“建筑原型”（Tavern/Residence/Workshop）。
3. 房间划分（BSP 简化版）
   - 对每个建筑生成 1-N 个房间节点；根据原型决定哪些房间可导航 `navigable=true/false`。
4. 路径图生成（连通优先）
   - 以建筑为图节点，构造最小生成树（MST）确保连通，再按概率添加少量冗余边；
   - 建筑→房间添加内部边，必要时设置单向（如厨房不可反向）。
5. 资源点配置
   - 基于建筑原型映射资源：Tavern→Food/Social，Residence→无或低强度 Social。
   - `capacity/rate` 按房间数、原型系数与随机扰动计算（可配置）。
6. 命名与标注
   - 采用内置词表 + seed 驱动的选择器生成可读名称；未来可本地化。
7. 校验与修复
   - 约束：图连通、无悬空 Spawn、`id` 唯一、父子层级合法。
   - 自动修复常见问题（如缺边/重复 id），失败时返回 `WorldLoadResult` 风格错误。
8. 序列化与注入
   - `LocationGraph → JSON`，保存到 `data/world/generated/<seed>.json`；
   - 或直接 `WorldRegistry.setGraph(graph)` 供运行时使用。

## 算法与实现要点
- 建筑布置：P0 采用规则/模板驱动（不做几何坐标），保持与现有 ASCII 布局解耦；
  - P1 可引入泊松盘采样/简单栅格坐标，仅用于可视化层。
- 房间划分：二叉空间分割（BSP）或模板枚举（厨房/大厅/寝室）。
- 路网：Kruskall/Prim 构造 MST，按分布增加冗余边，带 `cost` 权重。
- 资源分配：基于原型的规则（Rule + Weight），支持全局倍率调节。
- 随机性：集中封装 RNG，允许“冻结”序列以便测试与回放。

## 模块接口与集成
- 接口：
  - `struct GeneratorConfig { ... };`
  - `class IWorldGenerator { virtual LocationGraph generate(uint64_t seed, const GeneratorConfig&) = 0; }`；
  - `DefaultTownGenerator`：P0 实现，关注地点图与资源点。
- CLI 集成（`sandbox_cli`）
  - 新增命令：`world generate --seed <n> [--out <path>] [--config <path>]`
  - 无 `--out` 时直接注入运行时并渲染；提供 `--layout` 复用现有 ASCII 布局。
- 数据与配置
  - 约定目录 `data/world/generator/` 放置词表、原型、默认配置；
  - JSON Schema（后续）用于验证生成器配置。

## 测试与验证
- 单元测试
  - Seed 决定性：相同 `seed+config` 输出图同构（节点/边/Spawn 序列一致）。
  - 约束校验：连通性、父子关系、Spawn 归属、边权范围。
- 集成测试
  - `WorldLoader` 读取生成的 JSON 并成功装载；资源系统可在 demo 中正常产消。
- 基准测试（可选）
  - 规模从 10→100 建筑，统计生成时长与校验时长。

## 里程碑与任务拆分（建议）
1. P0 架子：接口与默认生成器（Region/Buildings/Rooms/Edges/Spawns）。
2. JSON 序列化与 CLI 命令 `world generate`。
3. 校验器与单元测试（决定性 + 约束）。
4. 建筑/房间模板扩展与资源分配规则抽象。
5. 可视化辅助（可选）：生成布局 JSON 供 ASCII 渲染。

## 非目标（本阶段不做）
- 连续几何地形/噪声地形渲染；
- 真实坐标系下的路径搜索与导航网格；
- 复杂剧情/任务链的世界嵌入（先保留接口）。

