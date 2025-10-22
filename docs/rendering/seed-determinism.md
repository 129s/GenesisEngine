# 种子与确定性（Seed & Determinism）

种子组成
- `seed = hash(worldSeed, sceneId, objectId, layerTag)`；
- `layerTag` 如：`material|seam|lighting|fitting|wear|decal|signage`。

锚定与相位
- Pattern 与 Dither 以 `phaseAnchor=(0,0)` 或对象原点锚定；
- 噪声函数（value/perlin/fbm）以 `seed` 初始化，`px,py` 取世界/对象坐标；
- 相机移动不改变图案相位，避免“纹理漂移”。

跨平台一致性
- 避免浮点不确定；优先使用整数或定点实现的噪声与阈值；
- 渲染顺序稳定：严格按 `procedural-pixel-art.md` 的流水线次序执行。
