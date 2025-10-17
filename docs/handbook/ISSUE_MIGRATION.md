# Issue 迁移指南（从 docs 到仓库 Issues）

## 目标
- 将 `docs/status/todo.md` 与 `docs/status/backlog/*` 的条目迁移为仓库 Issues，便于用 Milestones/Labels 追踪。

## 推荐方式
- 使用 GitHub CLI：`gh issue create`。
- 辅助脚本：`scripts/migrate_issues.ps1`（默认仅打印命令；加 `-Apply` 实际创建）。

## 用法
```powershell
# 仅输出命令（可复制到终端执行）
pwsh scripts/migrate_issues.ps1

# 直接创建（需要 gh 已登录且有权限）
pwsh scripts/migrate_issues.ps1 -Apply

# 指定仓库（组织/仓库）
pwsh scripts/migrate_issues.ps1 -Apply -Repo yourorg/GenesisEngine
```

## 约定
- Labels 建议：`area/<子系统>`, `P0|P1|P2`, 其他如 `telemetry`,`ux`,`guides`。
- 创建后在 `docs/status/todo.md` 勾选并附上 Issue 链接，或删除迁移条目，保持文档整洁。