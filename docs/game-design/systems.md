# 系统设计（Systems · 涌现导向）

事件总线（Event Bus）
- 一切行动产出结构化事件：`Event{step, actor, kind, locationId, payload, visibility}`；
- 作为“真相流”被日志/媒体/案件观察者订阅，支持回放与断言。

感知与日志（Perception & Logging）
- 可见性模型：距离/朝向/遮挡/人群噪声 → 逐句/主题/未捕捉；
- 日志条目携带：来源/时间/地点/可信度/可复核标记；
- 玩家/警方/媒体各自维护“所知图谱”，允许矛盾并存。

痕迹与证据（Evidence Tracker）
- 数据：`Evidence{ id, kind(physical|document|witness|record), source(locationId|actor), t, credibility, decay }`；
- 机制：行动落地生成/衰减/可伪造/可检验；
- 物理化：证据可被拾取/藏匿/移交，记录保全链。

案件观察者（Case Observer）
- 从事件模式聚合 Incident→Case，仅当“报案/曝光/目击阈值”满足才进入社会程序；
- 与警方案卷并存，允许玩家自建私人案卷。

机构行为（Institutions · 最小）
- 警局前台/档案/留置柜台：`ReportCrime/SubmitEvidence/QueryRecord/Sign` 皆需到场办理；
- 简化裁决：依据阈值与所知图谱评估“论证包”，产出现实后果与名誉变更。

证据板与知识图谱（Mind Map）
- 节点：证据/人物/地点；边：关系（因果/矛盾/同现）；
- 规则：仅做一致性提示与冲突高亮，不代判“对/错”。

时间线（Timeline）
- 数据：`EventRef{t, 地点, 当事人, 引用证据, 可信区间}`；
- 拖拽重建顺序，系统提示矛盾与缺口。

对话与对质（Dialogue & Confrontation）
- 话语原子：Ask/Answer/Claim/Refute/Accuse/Withhold/Gossip；
- 证据插入触发一致性检验与态度变化；
- 隐私等级影响被旁观/媒体捕捉的概率。

名誉与立场（Reputation & Faction）
- 名誉影响合作/价码/报道立场与机构阈值；误判会持续影响后续世界。

导演旋钮（Director · 软）
- 显露窗口/节流/误导率与证据暴露率档位；
- 仅调可见性与机会，不改事实。

场景与可见度（Fog & Light）
- 时段/天气影响可见性与巡逻；
- 光源与拥挤度作为观察成本。

最小对接清单（与引擎）：
- UI：
  - 证据板 2D 画布（节点/边/高亮）；
  - 对话框（可插入证据卡片）；
  - 日志与档案浏览；
- 数据：
  - Event/Incident/Evidence/CaseFile/KnowledgeEntry 的 JSON/内存结构；
  - 存档：证据板连接、时间线排序、名誉等。
