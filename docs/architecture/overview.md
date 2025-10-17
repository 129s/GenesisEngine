# GenesisEngine · Architecture Overview

目标：以可拓展、可观测的实时仿真内核为中心，向上提供一致的 Runtime 接口，向外暴露 CLI/GUI 等前端；底层以“分层节点图（World Graph）+ Tilemap（局部栅格）”表达世界，兼顾大域导航与局部渲染/碰撞。

## 分层结构（自下而上）
- Core/World
  - World Registry：分层节点图、资源点、边、生成/加载；提供拓扑与资源查询。
  - Systems：Movement、Resource、Action、Needs 等纯逻辑系统，按固定时钟推进。
  - Telemetry Buffer：只写端由核心填充、只读端供外界消费的环形快照。
- Runtime 封装
  - 单线程推进 Engine；队列化命令（暂停/步进/生成/注入事件），产出 Telemetry。
  - 并发边界：GUI/CLI 不触碰 ECS/Registry，仅通过 Telemetry/Atlas 读数据。
- 前端适配
  - CLI：文本渲染、命令交互。
  - GUI：GLFW+ImGui，消费 Telemetry 和 Atlas，绘制世界视图、面板与 HUD。

## 世界表达（双轨）
- 分层节点图（Layered Node Graph）：区域/建筑/房间/点；稀疏、可加权/有向，适于远距离导航与资源分布。
- Tilemap（局部栅格）：细粒度可碰撞的局部地图，可与“房间/点”绑定；适于局部路径、渲染与交互。

二者通过“锚点/门洞/传送口（Portal）”连接，形成跨图行走路径：Graph 做全局规划，Tilemap 完成局部落点与避障。

