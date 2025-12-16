# Proposal · 命名空间统一方案（2025-10）

> 目标：明确 GenesisEngine 代码库的命名空间标准，为后续逐步迁移提供依据。

## 现状与问题
- 核心模块沿用小写 `genesis::core / genesis::worldgen / Genesis::Runtime`，Sandbox GUI 也已调整为 `genesis::sandbox::gui`，而 Style 子系统仍保持首字母大写的 `Genesis::Style`，不同约定并存。
- 调用方在不同模块间切换时需要频繁添加双重 `namespace` 别名，降低可读性并增加出错概率。
- 文档与示例代码在引用命名空间时缺乏一致标准，阻碍团队对外输出和未来重构。

## 统一规范
- 采用首字母大写的 `Genesis::` 作为唯一根命名空间。
- 一级子命名空间遵循 PascalCase，例如 `Genesis::Core`、`Genesis::World`、`Genesis::Runtime`，当前已调整为小写的 `genesis::sandbox::gui` 将在最终阶段迁移至 `Genesis::Sandbox::Gui`。
- 测试、工具与脚本若需要引用内部实现，可在测试专用命名空间下使用 `Genesis::Testing` 等派生名称，避免污染顶层结构。
- 对于仍位于 `genesis::` 小写命名空间的模块，后续将通过代码迁移将其重命名至对应的 PascalCase 形式。

## 迁移策略
1. **引入过渡别名**：在公共头文件中添加 `namespace Genesis::Core = genesis::core;` 形式的别名，确保调用方可立即使用统一写法，同时保持 ABI 稳定。
2. **分模块重命名实现**：按照依赖拓扑，从底层到上层逐步重命名命名空间声明与使用点，优先处理头文件暴露接口的模块。
3. **清理过渡别名**：当所有调用点迁移完成后，移除过渡别名，实现完全统一。
4. **更新文档与示例**：同步修订 docs、示例代码与脚本，保证外部资料只展示新命名空间。

## 待迁移模块清单
- `genesis::core`（核心引擎）
- `genesis::agents`
- `genesis::world` 与 `genesis::worldgen`
- `genesis::planner`
- `genesis::telemetry`
- `Genesis::Runtime`（已完成 PascalCase）
- `genesis::messaging`

> 以上模块均需迁移至 `Genesis::` 前缀，并与现有 `Genesis::Style`、`genesis::sandbox::gui` 的最新状态保持一致。

## 配套行动
- 在后续重构任务中，创建针对命名空间迁移的专题分支，避免与其他功能改动交叉。
- 在代码审查流程中新增检查项：新增文件必须使用 `Genesis::` 命名空间。
- 为迁移引入的 alias 添加时间限制，定期检查并删除已无引用的别名，防止过渡状态长期存在。
