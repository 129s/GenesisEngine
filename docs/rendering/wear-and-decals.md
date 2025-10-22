# 磨损与贴花（Wear & Decals）

磨损（Wear）
- 边缘擦亮：距边界 1px 的区域按 `wear` 权重提亮/压暗；
- 刮痕：基于 seed 的稀疏短折线，角度与长度取离散集；
- 污渍：低频噪声阈值选取的斑块，叠加十字网点；
- 触点：把手/锁孔/抽屉口周围增加磨损权重。

贴花（Decals）
- 种类：印章/告示/破角贴纸/油渍/水痕；
- 混合：仅 `Dither` 或 `Pattern`（Checker/Hatch），不使用连续半透明；
- 遮罩：角撕裂用三角/锯齿mask；叠加在材质与高光之后。

确定性
- `seed = hash(worldSeed, sceneId, objectId, "wear|decal")`；
- 参数（频率/强度）受 Scene 主题与年代磨损程度调制。
