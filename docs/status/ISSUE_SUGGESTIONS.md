# 待迁移 Issues（建议）

> 说明：以下根据 `docs/status/todo.md` 与 `docs/status/backlog/*` 汇总，建议迁移为仓库 Issues（并绑定 Milestone/Label）。可使用 `scripts/migrate_issues.ps1` 生成 `gh issue create` 命令。

---

## 1) Runtime: 双缓冲 SimulationSnapshot（线程安全）
- Labels: `area/runtime`, `P0`, `telemetry`
- 参考：`docs/roadmap/README.md`（近期 P0），`include/genesis/runtime/Runtime.hpp`、`src/runtime/Runtime.cpp`
- 描述：
  - 在模拟线程与前端（CLI/GUI/调试）之间提供双缓冲快照结构，避免数据竞争；
  - 定义快照捕获点与生命周期；扩展 Runtime API 以获取稳定快照。
- 验收：
  - 在长时运行中无数据竞态；
  - CLI/未来 GUI 可安全拉取快照；
  - 单元测试覆盖并发读场景（可用假线程/多步验证）。

## 2) Sandbox CLI: 基本控制命令集
- Labels: `area/cli`, `P0`, `ux`
- 参考：`src/sandbox/CliApp.cpp`、`docs/guides/sandbox-cli.md`
- 描述：
  - 实现并完善命令：`pause`、`resume`、`step <n>`、`inspect agent <id>`；
  - `inspect` 输出代理位置、当前行动与需求（饥饿等）。
- 验收：
  - 脚本化执行可断言命令行为；
  - 文档更新，新增 `inspect` 使用说明与输出示例。

## 3) 指南：sandbox_ui / game 接入指引
- Labels: `area/docs`, `P1`, `guides`
- 参考：`docs/architecture/README.md`（运行时封装段）、`docs/guides/sandbox-cli.md`
- 描述：
  - 撰写面向前端的运行时接入指南（快照协议、基本 API、示例代码片段）；
  - 约定版本冻结点与向后兼容策略。
- 验收：
  - 新增文档 `docs/guides/runtime-integration.md`（或同类命名）；
  - 至少包含创建/步进/抓取快照/清理的最小可运行代码。