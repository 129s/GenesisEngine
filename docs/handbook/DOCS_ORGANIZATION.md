# 文档组织与边界约定（Roadmap/Status/Backlog/ADR/Changelog）

## 角色定义（Single Source of Truth）
- Roadmap（路线图）：面向未来的“做什么与为什么”。
  - 内容：愿景、主题（Themes）、里程碑与验收标准、时序与依赖。
  - 不包含：日常进度、短期任务清单、实现细节、PR 列表。
  - 更新节奏：阶段性（里程碑前后），保持稳定。
- Status（状态）：面向当前的“进行到哪里”。
  - 内容：按时间的进展记录（周报/双周报）、已完成/进行中/风险、下阶段重点；链接到 Issues/PR。
  - 不包含：长期路线与承诺（这在 Roadmap）。
  - 更新节奏：持续（每周/每迭代）。
- Backlog（待办）：任务收敛地，建议在 Issue Tracker（GitHub/GitLab）维护。
  - 文档中的 `docs/status/todo.md` 仅作临时收集，条目应尽快迁移到 Issue/Milestone。
- ADR（架构决策记录）：记录重要技术/架构决策及取舍。
  - 位置：`docs/adr/NNN-title.md`（可选采用 MADR 模板）。
  - 指向：相关的 Roadmap/Status 条目与实现 PR。
- Changelog（变更日志）：面向使用者的版本变更摘要。
  - 位置：仓库根 `CHANGELOG.md`（根据语义化版本更新）。
  - 不替代 Status；不记录内部探索性进展。
- Guides/Playbooks（指南）：操作性文档（构建、运行、工具、调试）。
- Architecture（架构）：系统设计、模型与集成方案。

## 目录结构建议
- `docs/architecture/`：世界模型、图→网格、Tile 融合、区块图、生成设计等。
- `docs/guides/`：CLI 使用、开发脚本、调试与测试指南。
- `docs/roadmap/`：统一路线图、里程碑拆分、历史路线图（只读）。
- `docs/status/`：进度概览、周报、临时待办（迁移到 Issue）。
- `docs/adr/`（可选）：编号决策记录。
- `CHANGELOG.md`（根）：用户可读的版本变更。

## 交叉链接规范
- Roadmap 仅链接到里程碑与高层设计，落地进展链接到 Status 段落与 Issues。
- Status 链接到 Roadmap 的对应目标、到具体 PR/Issue；避免复制 Roadmap 内容。
- 设计/RFC 完成后在 Roadmap 标记里程碑“已达成”，并在 Status 记录落地时间。

## 更新流程建议
1) 新目标/变更 → 先起草 ADR/RFC（如涉及架构），或直接在 Roadmap 草拟里程碑条目。
2) 拆分为 Issues/Milestones → 在 Status 跟踪迭代进展与风险。
3) 合并实现 → 更新 Changelog；在 Roadmap 勾选（或移动至历史）并在 Status 记录结果。