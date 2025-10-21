# Sandbox GUI · Smoke & RuntimeBridge 快速上手

本指南涵盖：
- 里程碑 1（Smoke）：验证 GLFW/OpenGL/ImGui 基础壳体。
- 里程碑 2（RuntimeBridge）：后台推进 Runtime、静态世界视图与首批面板。
- UI 文案已统一为英文（后续计划引入 i18n 切换）。

## 环境准备
- CMake ≥ 3.21
- 支持 C++20 的编译器（MSVC / Clang / GCC）
- Windows：建议安装最新显卡驱动；如果远程/无显示环境，运行时可能无法创建 OpenGL 上下文。

依赖管理通过 `CPM.cmake` 自动完成：GLFW 3.3、Dear ImGui 1.91.0 docking 分支。

## 构建步骤
```bash
cmake -S . -B build
cmake --build build --target genesis_sandbox_gui
```

首次构建会自动下载并编译第三方依赖，时间较长；后续增量编译仅受增量源码影响。

## 运行方式
可执行文件位于 `build/src/genesis-sandbox-gui.exe`（平台后缀视系统而定）：
```bash
./build/src/genesis-sandbox-gui.exe
```

启动后可看到：
- Docking 主视口（可自由拆分窗口）
- 顶部菜单（File / View），View 菜单可切换 Demo、Map View、Telemetry
- Welcome 面板：帧率、背景色、VSync、播放控制（Pause/Resume/Step/倍速）、最新快照摘要
- Map View 面板：静态世界拓扑图（节点/边/资源）与 Agent 名称标签（可选插值/轨迹/图例）。
- Scene View 面板：基于整数网格的场景观察（当前为网格占位渲染），支持：
  - 节点下拉选择（Room/Point/Building/Region）；
  - 资源点（local_coord）与锚点（anchors）标注；
  - 相机平移（右键拖拽）与缩放（滚轮）。
  - 网格/锚点/资源显示开关。
- Telemetry 面板：当前帧的 Agents/Needs/Actions 摘要。
- Inspector 面板：提供 Agent / Resource / Node 列表与详情（筛选统一由 Browser 负责），展示需求、行动、Planner 结果及 Runtime 事件，支持快速定位到 Map/Scene 以及跟随模式。
- 底部状态栏：快速统计（Step/Agents/Resources/Actions）

## World Generation 面板 · 命令队列

> 2025-10 起，世界生成/加载/保存通过 Runtime 命令队列执行，所有操作都异步排队并可追踪状态。

- 控件说明：
  - `配置路径` / `输出路径` / `随机种子`：作为 `world.generate` 命令的 JSON 负载提交。
  - `生成世界`：向命令队列提交 `world.generate`，返回事件 ID；命令成功后自动刷新最新种子，并在状态栏显示 `完成 (#id) · seed=...`。
  - `加载世界`：提交 `world.load`；成功后自动调用 `resetSceneForNewWorld()`，清空轨迹与 Inspector 选择。
  - `保存当前世界`：提交 `world.save`；成功信息写入 `RuntimeEventReport.message` 并显示在状态栏。
  - `命令脚本`：输入 JSON 文件路径（参考 `data/scripts/world_cycle.json`），点击 `执行脚本` 即可批量提交多条命令。脚本内命令支持 `waitForSuccess` 串联依赖。
- 面板底部展示命令队列状态表：
  - 列包含 `ID` / `标签` / `来源` / `状态` / `备注`。
  - 状态颜色：黄色（等待执行）、绿色（完成）、红色（失败）。
  - 队列视图默认折叠，可勾选 Pending/Succeeded/Failed 过滤，并支持 “Clear Completed” 隐藏已完成记录。
  - 当 `payloadJson` 留空时，备注显示 `message`；否则会回退到提交时的 JSON 片段。
- 命令执行流程：
  1. GUI 将 JSON 描述转换为 `RuntimeEvent`，通过 `RuntimeBridge::enqueueRuntimeEvent` 入队。
  2. Runtime 在下一次 `step` 前执行命令；执行结果通过 `SimulationSnapshot.events`、`RuntimeEventReport.message` 以及 `RuntimeBridge::commandStatusSnapshot()` 返回。
  3. GUI 使用事件 ID 更新状态字符串，并在成功时触发附加逻辑（例如刷新世界 Atlas、重置相机）。

### 命令脚本 JSON 速览

```json
{
  "name": "world-cycle-demo",
  "commands": [
    { "action": "world.generate", "configPath": "...", "waitForSuccess": true },
    { "action": "world.load", "path": "...", "waitForSuccess": true },
    { "action": "world.save", "path": "..." }
  ]
}
```

- `waitForSuccess: true` 表示该命令成功后才会继续提交下一条；失败会终止脚本并在状态表里展示 `失败 (#id) · message`。
- `label` 字段可选；未提供时默认等于 `action`，便于在状态表中分辨来源。
- Log Console 面板：捕获 `spdlog` 输出并持续滚动（默认开启，支持手动关闭/自动滚动）。
- 可选 Dear ImGui Demo 窗口（验证 Docking 与基础组件）

> ⚠️ 远程或无图形环境运行时，窗口可能无法创建；请在本地含 GPU/桌面会话的环境中执行。

## Runtime 控制与快照
- Control Toolbar（顶栏）集中 Pause/Resume、Step、Step ×10、Speed 与 VSync 控件，并响应 F5/F6/F7/F8/F9 快捷键。
- Welcome 面板保留状态信息与背景色调整。
- RuntimeBridge 在后台线程持续推进 `genesis::runtime::Runtime`，写入双缓冲 `SimulationSnapshot`（version/capturedAt/`TickTelemetry`，默认保留 96 帧）。
- GUI 线程每帧从快照缓冲读取最新数据，更新世界视图、Telemetry 与状态栏。

## Map View 面板
- 依据 `WorldRegistry` 中的 Location/Edge/Spawn 构建静态 2D 布局，按层级垂直排列。
- 节点颜色区分 Region / Building / Room / Point，并在圆心绘制类型图标；悬浮显示节点名称等信息，标签具备重叠抑制。
- `Agents` 复选框可开启/关闭代理标记，`Trails` 控制是否绘制近期轨迹，支援 4~64 个采样点调节。
- 行为图例区分 Move/Consume/Idle/Other 四类行动，标记颜色与 Log/Telemetry 信息保持一致。
- 节点选择：左键单击可选中节点并在画布上高亮，显示与父节点（金色）和子节点（蓝色）的连线；右键可清除选择，Inspector 中的 “Focus on Map” 会同步更新该选择。
- 仍为静态画布：暂不支持平移/缩放或实时布局更新（布局基于 `coord_global`）。

## 已知限制
- Map View 暂无摄像机/缩放交互；位置插值依赖 `movement_progress`（可切换 `Interpolate`）。
  - 在 Map 上点击节点可打开 Scene View 并选中对应节点；同一动作也会触发连线高亮。
- Inspector 仅展示需求/行动等基础信息，尚未接入人格 Big5、Traits、Portal 详情及历史趋势曲线。

## Scene View 面板（占位版）
- 以整数网格为单位渲染，像素 tileSize 仅影响可视大小；
- 资源与锚点基于 `spawns.local_coord` 与 `edges.anchors`；
- 后续将接入 Tilemap 纹理与图层，实现完整主观察渲染。
- Telemetry 面板仅展示当前帧摘要，尚无历史曲线、筛选或导出功能。
- RuntimeBridge 默认节流至 ~500Hz（2ms 休眠），缺少自适应帧率/实时性能指标。
- Agent Overlay 仅基于离散 LocationId，不含精确坐标/碰撞；多代理重叠时标记会遮挡。
- 出于线程安全考虑，暂未在 GUI 侧直接读取 ECS 组件进行“移动插值”。如果需要更平滑的轨迹，应当在 Runtime/Telemetry 中产出插值所需的稳定字段（例如当前段进度），GUI 仅消费 Telemetry。
- 未封装命令行选项；运行期间也未暴露世界重新生成/热加载入口。
- Headless 环境仍不支持运行（依赖 OpenGL 上下文）。
- Windows 下默认隐藏控制台窗口，若需查看原始日志请使用 Log Console 或附加自定义 sink。

## 下一步
- 里程碑 3：完善控制台板（播放/步进/倍率 UI）、HUD 指标与性能采样。
- 里程碑 4：Inspector 深化（人格/Traits、Portal 详情、历史曲线）与 Telemetry 图表。
- 里程碑 5：WorldGen 参数面板与世界热加载。
- 请跟踪 `docs/roadmap/SANDBOX_GUI.md` 获取后续任务进度与待办。

## 故障排查
- 若在某些终端环境运行 GUI 出现卡死或约第 200 步崩溃，请参考 `docs/troubleshooting/sandbox_gui.md`。

## Hotkeys
- F5: Pause/Resume
- F6: Step ×1
- F7: Step ×10
- F8: Toggle Log Console
- F9: Toggle Telemetry panel

Toast notifications pop at top-right for key actions (world generate/load/save, playback toggles).


