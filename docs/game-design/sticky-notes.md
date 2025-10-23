# 便签条与场景注记（Sticky Notes · 与证据板联动）

本文定义一套“便签条”机制，用于在世界显著位置贴放、标注与联想，服务于调查与记忆建模，同时与证据板（知识图谱）保持一致性。目标是在不侵入 Runtime 的前提下，于前端（GUI/Game）实现一个高性价比的玩法层。

## 设计目标
- 辅助记忆与梳理线索：将日志/证据/人物线索以“便签”形态落在具体地点/交互点上。
- 成为解谜工具而非纯 UI：可连线、成组、形成“思维网/证据链”，与证据板互相跳转。
- 保持设计铁律：不提供“指路灯”；仅做一致性提示与冲突高亮，不代判对错（与 `systems.md` 对齐）。
- 低侵入集成：前端 Overlay 实现；数据纳入“玩家知识存档”，不更改 Runtime 世界状态。

## 范围与非目标（MVP）
范围（做）：
- 便签创建/编辑/删除；贴放在 Map/Scene 画布上并锚定到实体或世界坐标。
- 便签之间的连线（直线/箭头）；颜色/标签/图钉样式；搜索与过滤。
- 与证据板互联：从证据/人物/地点卡片一键生成便签；从便签跳转到证据板节点。
- 存档与加载：纳入玩家知识层，随游戏存档恢复。

非目标（暂不做）：
- 系统自动推理与解答；强建议/自动连线（仅提供轻量聚类提示，见“灵感提示”）。
- 改变 NPC 行为或世界事实；便签仅表达玩家知识与意图。

## 数据模型（前端存储）
最小结构（JSON，归属“玩家知识层”）：

```jsonc
{
  "notes": [
    {
      "id": 1,
      "text": "南码头·午夜汽笛",
      "tags": ["地点", "午夜"],
      "color": "accent.primary.base",   // 调色板 token（见 guides/palette.md）
      "anchor": {
        "type": "entity|world",
        "entityId": 10002,              // Interaction/Portal/Resource 的 ID（三选一）
        "mapId": 1,
        "coord": { "x": 12.5, "y": 7.0 } // 当 type=world 时必填
      },
      "localTransform": { "rotation": 0, "scale": 1.0, "offset": [4, -2] },
      "style": { "paper": "lined|grid|plain", "pin": "red|blue|none", "durability": 1.0 },
      "links": [2],
      "createdAt": 1023456,
      "updatedAt": 1027890
    }
  ],
  "links": [
    { "from": 1, "to": 2, "kind": "relates|supports|contradicts", "strength": 0.6 }
  ],
  "settings": {
    "degradeDistance": true,            // 远距退化为 icon
    "respectKnowledgeFog": true         // Game 模式受知识雾影响（见 rendering/knowledge-fog.md）
  }
}
```

说明：
- `entityId` 对齐 `WorldAtlas` 中的 `Interaction/Portal/Resource` 等实体 ID；当锚在实体上时，便签随渲染端的实体投影移动。
- 颜色字段使用调色板 token（`guides/palette.md`），避免魔法数；若缺失则回退 `DesignTokens`。
- 存档版本化：为便签存档增加 `schema_version`，与 GUI 自身版本管理即可。

## 渲染与视觉（Overlay）
- 位置与缩放：在 `MapView/SceneView` 画布坐标系绘制；低缩放倍率退化为 icon（减少遮挡）。
- 纸张与贴花：复用 `rendering/signage-and-text.md` 与 `rendering/wear-and-decals.md` 的纸张/撕角/胶带视觉规范；禁用连续半透明，采用网点/有序抖动表现阴影。
- 字体与字号：采用 6×8/8×8 像素字库，行高为 8 的整数倍，禁用矢量抗锯齿（见 `rendering/signage-and-text.md`）。
- 颜色与状态：来自统一调色板；选中/悬停/禁用状态与 UI 状态矩阵一致（见 `rendering/ui-palette.md`）。
- 批渲染：便签矩形与连线采用 batched 提交；视区裁剪，减少 draw call。

## 交互与 UX
- 创建：
  - 从“日志/证据/人物卡”点击“生成便签”，默认锚到该实体或当前 Scene 中的点击位置。
  - 或在画布空白处双击创建空白便签，后续编辑文本/标签。
- 编辑：
  - 文本/标签/颜色/图钉样式即时编辑；允许拖动、旋转、缩放与吸附（对齐网格/角度）。
  - 连线：拖拽便签边缘手柄至另一便签形成连接；快捷键切换 `relates/supports/contradicts`。
- 搜索与过滤：
  - 侧边栏提供标签/颜色/锚点类型过滤；可临时“仅显示相关”。
- 与证据板联动：
  - 便签的“附着对象”（证据/人物/地点）以小图标显示；点击“在证据板中查看”跳转到对应节点。
  - 在证据板中可“生成场景便签”，快速把节点摘要贴回地图/场景。

## 模式与叠加层策略
- Sandbox 模式：便签完全可见且不受知识雾约束；提供调试面板（导入/导出、校验、清理）。
- Game 模式：遵循 `rendering/knowledge-fog.md`；未知区域便签仅显示占位/模糊标签；听闻未见则弱显。
- 叠加层与其它 Overlay 共存策略见 `rendering/overlays-and-modes.md`，避免泄漏 Sandbox 细节到 Game。

## 存档与版本
- 归属：玩家知识层，与“证据板/时间线/日志过滤状态”同一存档命名空间。
- 结构：`PlayerKnowledge{ notes[], links[], mindMap[], timeline[], ... }`（本页定义 notes/links 子集）。
- 版本：`notes.schema_version` 独立维护；GUI 升级时提供迁移脚本（例如颜色 token 重命名）。

## 事件与轻量“灵感提示”（可选）
- UI 标记事件：通过 `RuntimeEvent kind=Marker` 形式写入快照报告（不改变世界），用于回放/日志对齐。
- 聚类提示（可关）：
  - 评分：标签重叠（w1）+ 时间序相邻（w2）+ 空间邻近（w3）加权评分；超过阈值弹出“聚类泡泡”提示“可能主题：X”。
- 冲突高亮：当两便签被标注为 `contradicts` 且附着在同一实体/地点/时间段，给出冲突标记；与证据板冲突高亮一致化。

## MVP 范围与验收
- 能力：
  - 便签创建/编辑/删除；锚定（entity/world）；连线与过滤；与证据板双向跳转；存档/加载。
  - MapView/SceneView 中渲染与命中测试；低缩放退化为 icon；遵循主题配色。
- 性能：
  - 百级便签与连线在 60 FPS 下流畅；批渲染与裁剪生效。
- 验收：
  - 长跑（≥30 分钟）无内存泄漏；存档往复加载结果一致；在 Sandbox/Game 模式下视觉与可见性策略正确。

## 集成点与实现拆分
- 前端数据层：`NoteRepository`（GUI 内）；提供 CRUD、筛选、版本迁移、序列化。
- 叠加层实现：`NoteOverlay`，注册到 MapView/SceneView；封装绘制与命中测试。
- 桥接：`RuntimeBridge` 仅用于记录 Marker 事件（可选）；不访问 ECS。
- 调色板：依赖 `Genesis::Style::ColorRegistry`（见 `guides/palette.md`）。

## 风险与边界
- 视觉遮挡：通过低缩放退化、聚合/折叠与过滤缓解；避免覆盖关键交互提示。
- 玩法滥用：限制单场景便签数量上限（可配置）；提供批量隐藏与主题过滤。
- 协作/多端：本阶段不做多人同步；未来可引入“影子便签”只读展示。

