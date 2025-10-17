# Backlog · 世界生成（World Generation）

> 与 `docs/architecture/WORLD_GENERATION.md` 对应的实施任务拆分。

## P0（MVP）
- 定义接口：`IWorldGenerator` 与 `GeneratorConfig`（C++ 头文件与空实现）。
- DefaultTownGenerator：生成 Region/Buildings/Rooms/Edges/Spawns。
- JSON 序列化工具：`LocationGraph → JSON`（与现有 Loader 对齐）。
- CLI 命令：`world generate --seed <n> [--out <path>]`。
- 校验器：连通性/父子关系/Spawn 归属/权重范围。
- 单元测试：
  - Seed 决定性；
  - 约束校验；
  - Loader 读取生成 JSON 并通过 ResourceSystem 冒烟。

## P1（扩展）
- 建筑/房间模板库与规则系统（权重与条件）。
- 资源分配策略抽象与全局倍率。
- 生成布局导出：支持 `data/ascii_layout.json` 可视化。

## P2（可选）
- 简易坐标生成（栅格/泊松盘）与路径代价映射。
- 区域分区与门禁/单向边规则。

