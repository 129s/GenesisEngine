# 与世界数据对齐（WorldAtlas/Snapshot）

- Scene 尺寸
  - 缺省尺寸：子节点 WU 包围盒 + 1–2 WU 外扩；
  - 若生成管线提供 `footprint/scene_capacity`，优先映射为外观大小差异。
- Portal/Anchors
  - 箭头连接 Scene；`anchors` 在局部 WU 落点绘小方块；
  - `PathEdge.polyline` 仅 Sandbox 绘制（Game 关闭）。
- Interactive
  - 柜台/档案/告示栏/门禁等按其 WU 坐标放置字母标与几何底座；
  - 权限/状态以色/线型提示（禁区：斜线影/紫色）。
- 过滤
  - Game：按玩家知识做对象筛选（未见/听闻/已见）；
  - Sandbox：可切 truth/knowledge；允许真相叠加与探针。
