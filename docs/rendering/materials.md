# 材质库（Materials · Pixel Friendly）

说明：全部材质以“有限色带（ToneRamp）+ 解析噪声/图案 + 抖动”实现，禁止外部贴图。每个材质给出建议参数。

通用参数
- `tone: ToneRamp`（3–5阶），`angle`（度，主纹理方向），`freq`（像素周期），`turb`（扰动强度），`wear`（磨损权重），`seed`。

木材 Wood（P0）
- 纹理：沿 `angle` 的正弦/噪声条纹 + 低频 FBM；板缝 1px；钉帽 2×2px；
- 参数：`grainAngle`、`plankWidth`、`grainFreq`、`turbAmp`、`speciesPalette=oak|walnut|pine`。

石/砖 Stone/Brick（P0）
- Stone：细粒噪声 + 大块 cell 噪声混合；稀疏裂纹（细折线）；
- Brick：规则网格+错缝，单砖轻微明度抖动；砂浆 1px；
- 参数：`brickSize`、`mortarWidth=1`、`jitter`、`crackProb`。

金属 Metal（P1）
- 向异性条纹（沿 `angle` 的细线影）+ 高光带；铆钉 3×3px；
- 参数：`anisotropyAngle`、`sheen`、`rivetGrid{dx,dy}`、`oxidation`。

纸 Paper / 皮革 Leather / 布 Fabric（P1）
- Paper：极低频纤维点 + 微抖动；折边轻微高光；
- Leather：斑点 + 褶皱线；边缘擦亮；
- Fabric：斜向织纹（Hatch45 双向交错）。

玻璃 Glass / 水 Water（P1）
- Glass：边缘高光 1px + 低密度高光点；
- Water：波纹为正弦+噪声相位，反光高光带；
- 限制：不做半透明羽化；以抖动/高光线表达。

土/泥 Soil/Mud（P1）
- 大块噪声 + 点状颗粒；湿泥在上部叠加高光点阵。

瓷砖 Ceramic（P1）
- 规则格 + 细高光线；破损以裂纹线条表现；
- 参数：`tileSize`、`groutWidth`、`chipProb`。

调色板与色带建议
- Oak：`[#2B2116,#3A2A1C,#4A3626,#6B4B33,#8B6547]`
- Brick：`[#612D2D,#7C3A3A,#9E4C4C,#C06464,#E3A5A5]`
- Metal（铁）：`[#2A2F33,#3B4248,#56606A,#7B8794,#A7B1BC]`
- Palette 应统一在 16/32 色全集里选取，保持一致性。
