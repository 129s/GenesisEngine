# 世界生成器落地规划

## 目标
- 建立独立的 `genesis_worldgen` 库，实现完全代码驱动的世界生成流程。
- 交付可复现（`seed + 配置`）、可校验、可缓存的 Scene/Interactive Node 树与 Tilemap 资产。
- 与运行层/渲染层通过标准化数据契约对接，替换原型时期的手工/半自动流程。

## 交付里程碑

### 里程碑 A：基础骨架（已完成 2025-10-19）
- ✅ 建立 CMake 目标与目录结构（`engine/worldgen/*`），提供空壳 API。
- ✅ 实现配置加载与 Schema 校验占位（TOML 解析 + `world` 表校验）。
- ✅ 提供 deterministic RNG 工具、上下文骨架与日志能力。
- ✅ 编写首批单元测试（配置解析、RNG 确定性）。

### 里程碑 B：拓扑 + 网格布局（预计 2 周）
- 建立 CMake 目标与目录结构（`engine/worldgen/*`），提供空壳 API。
- 实现配置加载与 Schema 校验（TOML + JSON Schema）。
- 提供 deterministic RNG 工具、上下文、日志设施。
- 编写首批单元测试（配置解析、seed 复现）。

### 里程碑 B：拓扑 + 网格布局（预计 2 周）
- 完成 `TopologyModule`：支持簇状 + 线性组合拓扑，生成 SceneDraft。
- 实现 `GridLayout`、`ClusterLayout` 策略；支持 `scene_capacity`、boundary、最小间距硬约束。
- 接入 `ValidationModule` 基本检查：节点唯一性、越界、Portal 成对。
- 输出 `GeneratedWorld`（locations/edges/spawns），用于 Runtime 冒烟测试。

### 里程碑 C：多策略布局 + Tilemap 生成（预计 3 周）
- 新增 `HexLayout`、`NoiseRelaxationLayout`、`SplineCorridorLayout`、`VoronoiRegionLayout`。
- 引入 `TilemapModule` 与缓存机制，生成地形/碰撞层并写入快照。
- 扩展 `InteractionModule`：资源配额、Portal 锚点、楼层连接。
- 完善验证：可达性搜索、Tilemap 冲突检查、软约束评分。
- 建立金样回归测试（生成 → 序列化 → diff）。

### 里程碑 D：优化与工具支持（预计 1 周）
- 提供 CLI：`genesis-worldgen --config path --seed N --output world.json`。
- 引入属性测试框架（例如 rapidcheck）验证大样本稳定性。
- 统计并记录调试日志（重试次数、缓存命中率、约束违例）。
- 与运行层的端到端验证：加载示例世界并运行交互脚本。

## 主要任务拆解
- **API & 数据结构**：`GeneratorConfig`、`GeneratedWorld`、`SceneLayoutContext`、`ConstraintSet`。
- **模块实装**：Topology、Layout、Tilemap、Interaction、Validation、Export。
- **测试体系**：单元（策略/约束）、属性（随机种子）、回归（金样 diff）。
- **缓存方案**：磁盘目录规划、hash 设计、失效策略与日志。
- **文档**：对外 API 说明、配置范例、调试指南。

## 风险与缓解
- **策略多样性导致复杂度提升**：先交付网格/簇状最小集合，再逐步扩展策略，保持可测试性。
- **Tilemap 生成耗时/缓存不命中**：优先设计快速 hash 与增量更新；提供性能统计日志。
- **约束冲突导致生成失败**：增加调试模式输出候选点/被拒原因，必要时自动降级为软约束。
- **配置膨胀难维护**：制定 Schema 与约定命名，提供示例配置与注释模板。

## 依赖与准备
- 噪声/几何工具库选型（若自研不足可引入轻量第三方，需评估许可证）。
- 单元测试框架沿用现有 Catch2/GoogleTest（依项目基线而定）。
- Tilemap 快照格式与现有渲染管线对齐（参考 `sandbox_gui_tilemap_rendering.md`）。

## 成功判定
- 给定配置与种子多次生成 hash 一致。
- 运行层加载示例世界，交互闭环（资源、Portal）无异常。
- Tilemap 缓存命中率、生成耗时在可控范围（记录在日志中）。
- 回归测试覆盖主要策略，并具备持续集成钩子。
