# 模式差异与叠加层（Sandbox/Game）

- 共享叠加
  - 柜台队列状态、对话泡、名誉/关系轻量提示、冲突红点（证据/时间线一致性提示）。
- Sandbox 专用叠加
  - 节点ID、边/路径、显露/拥挤热力、探针文本（错误/警告/处理速率）。
- Game 专用叠加
  - 知识雾（未见弱化/抖动）、论证包一致性红点（提示冲突）
- 能力与策略
  - `mode=sandbox|game`、`knowledgeSource=truth|player|police|press`（Game 固定为 player）。
  - 构建剔除：Game 不链接 Sandbox 叠加与面板。
- 关联：详见 `docs/architecture/rendering-modes.md`（共享内核与策略总览）。
