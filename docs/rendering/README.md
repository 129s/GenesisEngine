# 渲染文档索引（像素风 × 零素材）

- coordinate-mapping.md — 坐标与单位映射（WU→RU、虚拟分辨率、缩放）
- core-primitives.md — 基础像素图元与抖动/渐变/字库规范
- composite-visuals.md — 组合图元（Scene/Portal/柜台/队列/对话泡）
- knowledge-fog.md — 知识雾的视觉与规则（Game 专用）
- overlays-and-modes.md — Sandbox/Game 模式差异与叠加层（与架构文档联动）
- performance-and-testing.md — 图层/批次/性能预算与确定性测试
- world-alignment.md — 与世界模型/Atlas/Snapshot 的对齐规则
 
// 程序生成像素美术（新增）
- procedural-pixel-art.md — 总览与流水线（材质/图案/遮罩/磨损/灯光/种子）
- materials.md — 材质库（木材/石砖/金属/纸皮革/布/玻璃/水/土等）
- patterns-and-dither.md — 图案与有序抖动库（Bayer 矩阵/阴影/网点）
- props-schema.md — 道具与设施的参数化 Schema（门/柜台/箱/桶/货架/路牌等）
- lighting-style.md — 像素化光照与AO（1px 高光/暗边/灯晕抖动）
- wear-and-decals.md — 磨损/污渍/裂纹与贴花生成
- signage-and-text.md — 标识与文本（招牌/海报/报纸/封印）
- seed-determinism.md — 种子与确定性（锚定/相位/哈希策略）

## 渲染脉络提示

1. 坐标与虚拟分辨率：`coordinate-mapping.md` 定义从世界单位到像素的映射、缩放与场景面板尺寸，是一切像素对齐的基础。
2. 基础绘制能力：`core-primitives.md` 与 `patterns-and-dither.md` 规定了像素级图元、调色与抖动手册，支撑“零素材”策略的可读性。
3. 语义化组合视图：`composite-visuals.md`、`world-alignment.md`、`knowledge-fog.md` 说明如何把世界语义映成 Scene/Portal/互动节点，并在 Game 模式下处理知识雾。
4. 模式与叠加层：`overlays-and-modes.md` 约束 Sandbox/Game 差异，保证调试层不会泄漏到正式玩法中。
5. 程序化像素美术流水线：从 `procedural-pixel-art.md` 的七步流程出发，细分为材质（`materials.md`）、图案（`patterns-and-dither.md`）、道具 Schema（`props-schema.md`）、灯光（`lighting-style.md`）、磨损贴花（`wear-and-decals.md`）、标识文本（`signage-and-text.md`）。
6. 性能与确定性：`performance-and-testing.md` 设定批次、图层和预算；`seed-determinism.md` 确保所有程序化元素在任意平台和重放下保持一致。

## 架构定位与集成

- 渲染库核心职责是把 `WorldAtlas` + `SimulationSnapshot` 投影成像素帧，遵循 `docs/architecture/rendering-modes.md` 中定义的 Renderer Core / Policy / Overlay 分层，向 Sandbox 与 Game 统一输出 `RenderFrame`。
- 运行层通过 Runtime API 提供只读数据，详见 `docs/architecture/overview.md`；渲染库不得回写逻辑状态，只消费坐标、标签、主题等标记。
- 零素材策略与像素风约束记录在 `docs/architecture/zero-asset-rendering.md`，渲染库需要内建这些规则并对外提供材质/图案/调色板等参数化接口。
- Sandbox GUI 依赖同一库的相机/命中测试/批次管理（参考 `docs/architecture/sandbox_gui_tilemap_rendering.md` 与 `sandbox_gui_scene_unification.md`），Game 客户端复用核心并替换策略与叠加层。
