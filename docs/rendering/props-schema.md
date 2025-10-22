# 道具与设施 · 参数化 Schema（Parametric Props）

目标：用一套统一的 Schema 描述“门/柜台/箱/桶/货架/路牌”等像素道具，渲染端据此程序生成外观。

通用字段（所有道具）
- `id`、`seed`、`px,py`（像素位置，或 `coord_global` 映射后像素）、`w,h`（像素尺寸）
- `geometry`：矩形/圆角矩形/多边形 + 可选分片（panelGrid）
- `materials[]`：层数组（base/edge/fittings/label），引用 `materials.md` 中的材质+参数
- `fittings[]`：五金与细件（铆钉/把手/锁舌/铭牌等）
- `decals[]`：贴花（印章/贴纸/海报小角标），含 `mask` 与 `blend=Dither|Pattern`
- `wear`：磨损参数（边缘强度/触点分布/刮痕概率）
- `state`：开合/选中高亮等动画态（0..1）

示例 Schema 片段
- Door（门）：
  - `geometry:{ kind: RRect, r:2, panels:{rows:2, cols:1} }`
  - `materials:{ base:Wood{oak..}, edge:Wood{darker}, fittings:Metal{iron} }`
  - `fittings:[ handle{pos:…}, keyhole{pos:…} ]`
  - `wear:{ edge:0.6, scratches:0.2 }`
- Counter/Cabinet（柜台/柜）：
  - 上板/立面分层；抽屉拉手阵列（2×1px）；标签卡槽
- Crate/Barrel（板条箱/木桶）：
  - 板条/金属箍分层；铆钉阵列；木纹参数化复用
- Signboard/Poster（招牌/海报）：
  - 文字内容与边框样式；纸张材质；破角贴花

渲染顺序约束
`mask → base material → seams → lighting/AO → fittings → wear → decals → signage`

确定性与主题
- `seed = hash(worldSeed, sceneId, objectId, layer)`；
- Scene `metadata.theme` 可影响材质默认：港口=橡木+铁件；会馆=胡桃木+黄铜等。
