# MVP 任务拆解（P0）

说明：按 Runtime / Sandbox / Game 三层划分，列出任务包、验收标准（DoD）、依赖与风险。与 `mvp.md` 的里程碑（M0–M3）对应执行。

## Runtime（权威模拟层）

- R1 事件总线（Event Bus）
  - 内容：定义 Event 结构（kind/step/actor/location/payload/visibility）、在 ActionExecutor 发射；写入 Snapshot。
  - DoD：有稳定 schema；能在日志/回放中看到事件序列；同一种子重放一致。
  - 依赖：无；为后续模块提供输入。

- R2 可见性标注与采样（Perception Tags）
  - 内容：为 Event 标注可见性元数据（距离/朝向/遮挡/拥挤 → 逐句/主题/未捕捉）。
  - DoD：Game 层同一事件在不同位置/姿态下得到不同粒度日志。
  - 依赖：R1；与 Sandbox 探针联动验证。

- R3 痕迹与证据（Evidence Tracker）
  - 内容：事件→EvidenceFact（physical/document/witness/record）；衰减/可伪造/保全链；拾取/藏匿/移交动作。
  - DoD：EvidenceFact 可查询/落在具体地点；保全链记录完整；提交到柜台可被接收。
  - 依赖：R1/R2。

- R4 机构柜台与队列（Institutions: Police Counters）
  - 内容：前台/档案/留置交互点，时段/权限/队列/处理速率；命令 `counter.report/submit/query/sign`（到场校验）。
  - DoD：NPC/玩家到场排队办理；拒绝原因明确（超时/缺材料/无权限）；处理结果写入事件与案卷。
  - 依赖：R1/R3；WorldAtlas 暴露柜台位置。

- R5 案件观察者（Case Observer · 最小）
  - 内容：模式聚合 Incident→Case，触发条件（报案/目击/曝光阈值）；生成 CaseFile 外壳。
  - DoD：自然发生 ≥1 起盗窃/破坏被聚合为 Case；CaseFile 与柜台流程关联。
  - 依赖：R1/R3/R4。

- R6 名誉与后果（Reputation & Outcome）
  - 内容：提交“论证包”后，依据 Runtime 所知图谱阈值裁定（简化裁决），写入名誉与人物命运变化。
  - DoD：错误结论导致名誉下降和人物状态变更；被日志/媒体记录。
  - 依赖：R5。

- R7 Director 旋钮（软导演）
  - 内容：显露窗口/节流/误导率/证据暴露率参数；仅 Runtime 生效，Sandbox 可调，Game 用预设。
  - DoD：Sandbox 面板调参对事件显露与频度产生可观测影响；不改既有事实。
  - 依赖：R1–R5。

- R8 存档（World State）
  - 内容：保存/加载世界状态（节点/队列/案卷/证据/名誉），与种子复现一致。
  - DoD：存取后事件统计与案卷一致；种子重放一致。
  - 依赖：前述模块稳定。

## Sandbox（开发观测层）

- S1 事件/快照浏览器
  - 内容：按时间查看 Event 序列、过滤/搜索、对比回放；
  - DoD：可定位任一事件，支持按 actor/location 过滤；截图支撑文档。
  - 依赖：R1。

- S2 真相叠加与探针
  - 内容：拥挤/风险/显露热力、柜台队列与处理速率可视化、节点ID/路径显示。
  - DoD：叠加层随时间变化，能验证 R2/R4/R7 的效果。
  - 依赖：R2/R4/R7。

- S3 Director 面板与脚本
  - 内容：旋钮热调（显露/节流/阈值）、命令脚本执行、快照回放入口。
  - DoD：参数调整实时生效；可执行最小脚本链路（生成→加载→运行）。
  - 依赖：R7。

## Game（正式玩法层）

- G1 地图与标记（最小美术）
  - 内容：彩色矩形场景 + Portal 箭头；NPC/柜台/证据字母图标；对话泡（逐句/主题两档）。
  - DoD：无指路灯；随快照刷新；基本交互反馈。
  - 依赖：WorldAtlas/SimulationSnapshot。

- G2 日志视图（玩家知识）
  - 内容：按事件（过滤可见性）生成逐句/主题条目；来源/可信度/可复核；检索与标注。
  - DoD：同一事件随距离姿态产生不同粒度条目；可筛选与定位来源。
  - 依赖：R1/R2。

- G3 证据板（知识图谱）
  - 内容：节点（证据/人物/地点）与连线（因果/矛盾/同现）；冲突高亮；标签与简注。
  - DoD：可手动连线；系统给出一致性提示，不代判对错。
  - 依赖：R3。

- G4 时间线（重建校验）
  - 内容：拖拽事件顺序；提示同地不同时间/同人不同地点/因果缺口冲突。
  - DoD：能用已有 Evidence/EventRef 重建关键片段。
  - 依赖：R1/R3。

- G5 柜台交互流
  - 内容：到场→排队→办理（`report/submit/query/sign`）；拒绝原因展示；材料清单提示（不指路）。
  - DoD：玩家亲赴办理并得到明确结果；日志留痕；与案卷状态联动。
  - 依赖：R4。

- G6 问答/指控（证据插入）
  - 内容：话语原子 Ask/Answer/Claim/Refute/Accuse；插入证据影响态度与信息层级。
  - DoD：对话可引用 Evidence；态度与信息可见变化。
  - 依赖：R3。

- G7 论证包提交与后果展示
  - 内容：整理时间线+动机闭环+关键物证≥N；向柜台提交；展示裁定/评级与后果/名誉变更。
  - DoD：近似正确得到评级；错误结论导致现实后果，媒体/日志记录。
  - 依赖：R5/R6。

- G8 玩家侧存档（Player Knowledge）
  - 内容：保存日志/证据板/时间线/名誉与资源；加载一致。
  - DoD：存取后知识层一致；与 Runtime 存档协同。
  - 依赖：R8。

## 共享渲染（Renderer Core & 模式差异）

- XR1 渲染内核抽取（共享）
  - 内容：抽取 Map2D 图元/相机/命中测试/主题为可复用模块；Sandbox/Game 共用。
  - DoD：两端均可渲染彩色矩形/Portal/图标/对话泡；视觉差异仅来自策略与叠加层。
  - 依赖：WorldAtlas/SimulationSnapshot。
  - 参考：`docs/rendering/coordinate-mapping.md`、`docs/rendering/core-primitives.md`

- XR2 可见性策略与过滤链
  - 内容：实现 VisibilityPolicy（Sandbox 可切换 truth/knowledge；Game 绑定 player）与 FilterChain（知识过滤）。
  - DoD：Sandbox 可切换真相/知识视图；Game 仅显示玩家知识层。
  - 依赖：R2、XR1。
  - 参考：`docs/rendering/overlays-and-modes.md`

- XR3 叠加层插件化
  - 内容：OverlayProviders 注册/开关；Sandbox 专用（ID/边/热力/探针）、Game 专用（知识雾/一致性红点）。
  - DoD：Sandbox 叠加可逐项开关；Game 仅加载自身叠加。
  - 依赖：XR1、XR2。
  - 参考：`docs/rendering/overlays-and-modes.md`

- XR4 构建能力与剔除
  - 内容：能力开关（capabilities）与构建剔除（Game 不链接 Sandbox 插件与面板）。
  - DoD：Game 构建不包含真相叠加/参数面板；Sandbox 可全功能调试。
  - 依赖：XR3。

- XR5 知识雾（Game）
  - 内容：未见过节点弱化/隐藏的视觉规则，不改变逻辑可达性。
  - DoD：玩家未见区域弱化显示；见闻后恢复正常渲染。
  - 依赖：XR2、XR3。
  - 参考：`docs/rendering/knowledge-fog.md`

- XR6 零素材主题与图案库
  - 内容：配色/线型/网点与斜线影/渐变的参数化集合；不使用纹理。
  - DoD：可切换主题；不同状态（警戒/嫌疑/法务）有一致视觉表达。
  - 依赖：XR1。
  - 参考：`docs/rendering/core-primitives.md`

- XR7 图标字母映射与图例
  - 内容：职业/交互点/证据类型→字母/几何组合映射；图例面板（Game/Sandbox 共用）。
  - DoD：图例清晰、易于记忆；地图悬停可显示解释。
  - 依赖：XR1、XR3。
  - 参考：`docs/rendering/core-primitives.md`、`docs/rendering/composite-visuals.md`

- XR8 可访问性
  - 内容：高对比主题、红绿弱化模式、字号缩放；
  - DoD：切换即时生效，文本对比满足标准，图例在色弱模式可辨。
  - 依赖：XR6、XR7。
  - 参考：`docs/rendering/core-primitives.md`

- XR9 程序化材质（P0）
  - 内容：实现 `Wood`、`Brick/Stone` 基础材质（色带+噪声+抖动+接缝）。
  - DoD：木板地面/砖墙能在像素风下稳定渲染（含板缝/砂浆/裂纹概率）。
  - 参考：`docs/rendering/materials.md`、`docs/rendering/patterns-and-dither.md`

- XR10 参数化道具 Schema（P0）
  - 内容：门/柜台/板条箱的最小 Schema 与渲染顺序（mask→material→seams→lighting→fittings→wear→decals）。
  - DoD：同一 Schema+seed 在不同平台重现一致外观；参数变更即时可见。
  - 参考：`docs/rendering/props-schema.md`、`docs/rendering/procedural-pixel-art.md`

- XR11 像素化光照与磨损（P0）
  - 内容：1px 高光/暗边、接缝 AO；边缘擦亮/刮痕/污渍按 seed 生成。
  - DoD：统一光向；磨损强度可调；无模糊阴影与连续透明。
  - 参考：`docs/rendering/lighting-style.md`、`docs/rendering/wear-and-decals.md`

- XR12 确定性与锚定（P0）
  - 内容：图案/抖动相位锚定；种子哈希策略；避免相机移动导致纹理漂移。
  - DoD：相同 seed/对象在多帧与不同机器上一致；滚动/缩放不破坏相位。
  - 参考：`docs/rendering/seed-determinism.md`

## 里程碑映射（对 `mvp.md`）
- M0：R1/R2 + S1 + G1/G2
- M1：R3/R4 + S2 + G5
- M2：R5/R6 + G3/G4/G7
- M3：R7/R8 + S3 + G8（及媒体显露相关展示）

## 风险与回退
- 涌现不足：优先通过 R7 提升显露窗口与机会密度；不捏造事实。
- UI 复杂度：证据板/时间线保持平面直线与基础标注；样式延后。
- 性能：活跃区高保真，外围用统计近似；探针开关可降开销。

## 非目标（MVP 不做）
- 完整法庭/证据排除细则、身份伪装与误认、交通载具、媒体更正长链等（见 `top-level-design.md` 的 P1/P2）。
