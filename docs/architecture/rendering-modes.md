# 渲染模式与共享内核（Sandbox/Game 共用 · 零素材）

目标：零美术素材前提下，复用一套 2D 地图渲染内核（彩色矩形/Portal/字母图标/对话泡），以“模式策略 + 叠加层插件 + 数据过滤”区分 Sandbox 与 Game 的呈现与信息边界。

## 共享渲染内核（Renderer Core）
- 输入：`WorldAtlas`（Scene/Portal/柜台等静态）、`SimulationSnapshot`（动态对象/事件）、主题（颜色/字号）。
- 基础图元：Scene 彩色矩形、Portal 箭头、NPC/柜台/证据字母图标、文本标签、对话泡（逐句/主题两档）。
- 布局与相机：统一平移/缩放/裁剪与命中测试；输出 `RenderFrame{ drawLists[], hitTestIndex, stats }`。

## 模式策略（Policies）
- VisibilityPolicy（可见性源）：
  - Sandbox：`truth|player|police|press` 可切换；
  - Game：固定 `player`（玩家知识层）。
- ThemePolicy（主题）：
  - Sandbox：调试主题（高对比、密信息）；
  - Game：叙事主题（报刊/档案风格）。

## 叠加层插件（Overlay Providers）
- 共享：柜台队列状态、对话泡、名誉/关系提示（压缩）。
- Sandbox 专用：节点ID、边/路径、事件/显露热力、探针文本（错误/警告/处理速率）。
- Game 专用：知识雾（未见弱化/隐藏，图案填充/透明度降低）、论证包一致性红点（仅冲突提示）。
- 能力开关：`capabilities = { overlays: {...}, knowledgeSource, mode }`。

## 数据过滤（Filter Chain）
- 源一致：均来自 Runtime 的 Atlas/Snapshot。
- Game：先走“知识过滤器”（玩家日志/见闻/报刊 → 可见集合）再渲染。
- Sandbox：可切换 `truth/knowledge`，允许叠加调试层。

## 隐私与边界
- Game 禁止显示未被玩家掌握来源的字段（如真实罪犯标识、隐藏 Portal 锚点）。
- Sandbox 的真相叠加/探针不进入 Game 构建（构建时剔除）。

## 实施顺序（MVP）
1) 抽取 Map2D 渲染内核（图元/相机/命中测试/主题）。
2) 实现 VisibilityPolicy 与 FilterChain（Game 绑定玩家知识；Sandbox 可切换）。
3) 插件化 OverlayProviders；Sandbox 专用热力/探针可按需开关。
4) 能力开关与构建剔除（Game 不包含 Sandbox 插件与调试面板）。
5) 知识雾（Game）：未见过节点弱化/隐藏；不影响逻辑访问判定。

## 零素材实现要点（补充）
- 只使用几何图元、程序化渐变/噪声，不依赖外部纹理与图集；
- 字体：若不允许外部字体，使用内置矢量笔画字体或系统字体回退；
- 图标：以字母/几何组合与图例说明替代；
- 视觉风格通过配色/线型/版式与轻量动效建立。
