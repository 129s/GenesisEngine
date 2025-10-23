# 程序生成像素美术 · 总览与流水线

目标：在“全分辨率 × 像素风 × 零素材”约束下，用基础图元和程序化材质生成可复用的像素风美术，覆盖场景、道具与标识，不依赖外部贴图。

流水线（每个对象/片区的绘制顺序）
1) 几何遮罩（Geometry Mask）：用矩形/多边形/圆角矩形勾勒对象体块，限制后续填充范围。
2) 基础材质填充（Material Fill）：调用材质函数（如 Wood/Brick/Metal），按有限色带+抖动生成纹理。
3) 分片/接缝（Panel/Seams）：木板缝/砖缝/金属带/砂浆等，1px 线条与轻微色阶差。
4) 光照与AO（Lighting/AO）：按统一光向（建议左上→右下）加 1px 高光/暗边、接缝 AO 阴影（线影或网点）。
5) 五金与细件（Fittings）：铆钉/锁舌/把手/铭牌等几何小件（2–3px 单元）。
6) 磨损与贴花（Wear/Decals）：擦边、划痕、污渍、裂纹、印章/告示贴纸等（受 seed 与位置驱动）。
7) 标识与文字（Signage/Text）：字母/单字图标、招牌与标签（像素字库）。

基本约束
- 均使用整数像素坐标/线宽；禁用抗锯齿与连续半透明；渐变以“色阶+有序抖动”实现。
- 图案与噪声的“相位锚定”固定在世界/对象原点，避免相机移动时花纹漂移。
- 颜色来自小型色带（3–5 阶），全局调色板 16/32 色，便于统一风格。

接口约定（伪）
- `MaterialFn(px, py, seed, params) -> shadeIndex|rgba`（基于解析噪声/模式）；
- `ToneRamp{steps[], paletteId}`（色带定义）；
- `Pattern{kind:Hatch45|Cross|Checker|Dots, phase, density}`（图案）；
- `WearMask(px, py, seed, params) -> 0..1`（边缘/手触区域加权）。

与世界数据对齐
- 种子：`seed = hash(worldSeed, sceneId, objectId, layerTag)`；
- 场景主题：Scene/Interactive 的 `metadata.theme/material` 指示材质与参数范围；
- 生成结果不回写世界模型，仅渲染层消费。

## 库拆分建议：生成 vs 渲染

- **素材生成库（Procedural Asset Library）职责**
  - 提供材质、图案、磨损、贴花等生成函数，输出中立的 `PixelLayer/Mask` 或参数化描述（例如 `MaterialDescriptor`、`DecalDescriptor`）。
  - 负责 seed 管理、相位锚定、ToneRamp/Palette 查表以及缓存策略（同一 seed + 参数组合只生成一次，供多个对象复用）。
  - 对外暴露纯函数式接口，便于离线预烘焙或测试；不依赖渲染上下文。
- **渲染库职责**
  - 读取 `WorldAtlas`/`SimulationSnapshot`，结合素材生成库产出的素材描述，完成 draw list 组装与像素输出。
  - 管理图层顺序、批次、相机、Overlay 与策略；对素材只关心“怎么摆放”和“在哪个批次绘制”。
  - 允许通过依赖注入选择具体素材生成实现（默认零素材版本，可扩展为自定义材质包）。
- **接口建议**
  - 定义中间表示（IR）：`struct ProceduralAsset { PixelLayer layers[]; Rect bounds; Metadata tags; }`，渲染库按场景需求把 IR 投射到最终像素。
  - 支持懒加载：渲染库按需请求素材，生成库依据参数返回缓存引用或现算结果。
  - 保持确定性：生成库必须满足 `f(params, seed) -> deterministic output`，渲染库负责在帧内复用相同引用，避免重复生成。

参考
- 材质库详见 `materials.md`；图案与抖动见 `patterns-and-dither.md`；道具 Schema 见 `props-schema.md`；灯光与磨损见 `lighting-style.md`、`wear-and-decals.md`；确定性策略见 `seed-determinism.md`。
