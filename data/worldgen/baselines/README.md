# Worldgen Baselines（基准世界配置）

这些配置用于生成“回归/Soak/指标对比”的基准世界集合：以 `world.db.generate` + 固定 seed 生成可复现的 `world.json + map_{id}.json` 产物，再用 `genesis-runtime-cli soak` 输出软指标报告（多样性 / 资源经济）。

原则：
- 基准世界不是 Demo：它们用来覆盖关键机制边界与统计形态（多样性、资源压力），并尽量保持小而稳定。
- 输出应可复现：`config + seed` 固定时产物与统计应稳定（至少在 Windows 优先环境下）。

建议配套脚本：`scripts/run_baseline_soak.ps1`

当前基准集合：
- `scarcity.toml`：单资源（Food），无 regen；验证长期缺资源下的目标选择与耗尽行为。
- `abundance.toml`：Water/Food 充裕；验证稳定供给下的低压力形态。
- `diversity.toml`：Water/Food/Social 混合、多地图；验证分流与多样性基线。
- `crowding.toml`：Water/Food + 多 agent；验证拥挤下 planner 分裂。
- `ecosystem_v1.toml`：引入工坊配方（Food 依赖 Water；Social 依赖 Food+Water），要求 agent 执行生产动作，作为“最小生态闭环”第一版。
- `ecosystem_v2.toml`：引入 `Ore` + 多配方工坊（替代输入）；观察瓶颈转移与“转产”倾向。
- `substitution_v2.toml`：强化 `Ore` 稀缺下的替代配方；观察在不同压力下是否切换到低产但稳定的 Water-only 路线。
- `logistics_v2.toml`：使用 `map_overrides` 将 Water/Ore/Food/Social 空间分离；观察迁徙、补给线与跨地图拥堵形态。
- `tool_feedback_v3.toml`：引入 `Tool` 作为中间品与“生产力放大器”（配方用 Tool 提升产出）；观察正反馈扩张与上游压力导致的瓶颈迁移。
  - 启用 `Food` 腐败：`decay_per_step=1` + `decay_types=["Food"]`，用于制造周期性补给压力与浪费。
