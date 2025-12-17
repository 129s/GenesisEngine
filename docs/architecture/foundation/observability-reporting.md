# 可观测性汇报约定（离线）· v0

目标：我负责跑批与回归，并用**可复现的证据**向你汇报“世界如何演化、哪些对象发生了变化、变化点在哪里”。

## 1. 一键跑批入口

- 批量生成（worldline + eventline + 报告）：`scripts/run_observability_batch.ps1`
- 产物位置：`out/observability/batch_*/reports/`

## 2. 我对你汇报的固定结构（默认）

1) **运行配置**：worldgenConfig / seeds / steps / agentIndex（观测对象）
2) **参考点**：跨 seeds 的分布（median/IQR），用于让数字“有尺度”
3) **差异定位**：
   - Worldline：宏观窗口转折候选（不命名、不分章）
   - Eventline Compare：单对象的变点窗口与社交集中度（topPartner share）
4) **证据回指**：把“最值得看”的 1–3 个窗口/片段指向对应报告文件（便于你抽查）

备注：runtime eventline 只写事实；任何推断/归因只在离线 lens 中做，并且在汇报中明确标注为“派生/推断”。

