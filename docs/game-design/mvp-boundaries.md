# MVP 边界划定（Runtime / Sandbox / Game）

目标：明确三层职责，避免功能越界；MVP 以“游戏=交互/表现壳”为前提。

## Runtime（权威模拟层）
- 职责
  - 世界结构与行为：`Scene/Interactive`、连通边、Needs/Planner/Movement。
  - 涌现支撑：Event Bus（真相流）、Evidence Tracker（痕迹/衰减/保全链）、Case Observer（Incident→Case 聚合）。
  - 机构最小流程：警局前台/档案/留置柜台 + 队列/时段/权限校验；`Report/Submit/Query/Sign` 等命令在此判定。
  - 契约与快照：`SimulationSnapshot` / `WorldAtlas`（只读）、`RuntimeEventReport`（命令结果）。
- 不做
  - 不包含侦探专用 UI/评分/结案打分逻辑；不提供“远程报案/菜单办理”。

## Sandbox（开发观测层）
- 职责
  - 调试/观测：事件流与遥测、真相叠加（热点/拥挤/风险）、柜台队列/处理速率、路径与节点ID。
  - 运维工具：参数面板（Director 旋钮：显露/节流/阈值）、脚本执行、快照回放/断言。
- 不做
  - 不承载正式玩法 UI；展示的“真相层”信息不在 Game 暴露。

## Game（正式玩法层）
- 职责（交互/表现壳）
  - 地图呈现：彩色矩形+Portal 箭头；NPC/柜台/证据为字母图标；对话泡文本。
  - 侦探工具：日志（逐句/主题，来源/可信度/可复核）、证据板（知识图谱，仅一致性提示）、时间线（冲突提示）。
  - 交互流：问答/指控（证据插入）、柜台交互（到场校验）、雇佣/贿赂（通用筹码）。
  - 名誉与资源展示：声望对合作/价码/报道倾向的影响可见。
  - 难度：绑定预设 Director 参数（不可随意调参）。
- 不做
  - 不展示真相叠加/内部探针；不提供生成/参数面板。

## 存档
- 双层：World（Runtime） + Player Knowledge（日志/证据板/时间线/名誉）。

