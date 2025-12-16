# TODO（短期可执行，尽量去噪）

> 本清单只保留“当前主线”可执行事项（建议控制在 5~8 条）。长期/大块计划请看 `../roadmap/README.md`；细分任务建议迁移到 Issue/Milestone。

## 主线（建议 2 周内）
- [ ] Core Regression：补齐端到端闭环断言（NeedSatisfier → ActionExecutor → Need 恢复），并确保 `scripts/run_regression_core.ps1` 能稳定复现与输出摘要。
- [ ] Soak 基准集：固定一组 `preset + worldgen config + seeds` 作为对照集合，产出 `out/metrics` 并用 `scripts/compare_soak.ps1` 生成差异摘要（不门禁，强留档）。
- [ ] Worldline Chronicle：补齐/校准宏观窗口特征（阶段/变点提示、分布漂移摘要），并确保 `scripts/compare_worldline.ps1` 输出可定位窗口。
- [ ] 母题工具链 v0：让 `scripts/analyze_worldline_motifs.ps1` 的输出能回指到“窗口/证据”，便于人工裁剪与入库治理（见 `docs/architecture/meta/motif-governance.md`）。
- [ ] 文档对齐扩面：持续把“设想/承诺”迁入 `docs/architecture/proposals/`，并保持 docs 无断链与“未落地写成已实现”。

## 暂缓（只留入口）
- Sandbox GUI：暂缓推进，见 `docs/roadmap/SANDBOX_GUI.md` 与 `docs/status/history/2025-12-15_gui-runtime-ux.md`。
- Scene/Interactive 重构：见 `docs/roadmap/MVP_SCENE_INTERACTIVE.md`。




