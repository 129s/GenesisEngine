# GenesisEngine 重构蓝图（架构与核心模块）

> 版本：2025-10-23  
> 作者：Codex（协同清扫）  
> 目的：指导后续大规模架构整顿、模块拆分与构建系统调整。

## 1. 背景与目标
- 现状：核心逻辑挤压在单一静态库 `Genesis::Engine` 与超大实现文件（例如 `RuntimeBridge.cpp`、`AppHostCore.cpp`）。运行时对外暴露引擎内部对象，GUI 与工具链直接依赖实现细节，构建过程中资产生成与测试紧耦合。
- 清扫目标：
  - 重建分层结构，明确模拟内核、运行时服务、界面层之间的界限。
  - 提升模块化程度，让构建目标与命名空间映射清晰，支持并行开发与替换。
  - 维持现有单机沙盒功能，同时为后续拓展（多人、脚本、远程调试等）保留扩展缝隙。
  - 让测试体系对齐新模块，降低单测对实现文件的直接引用。

## 2. 设计原则
1. **分层依赖**：任何模块只依赖同层的基础服务或下一层，不允许跨层取用实现细节。
2. **稳定接口**：向外暴露 Facade 或纯接口，内部实现可迭代；运行时不再泄露引擎内部对象。
3. **数据驱动**：调色板、世界生成等资产由独立服务负责；消费端通过 API 访问元数据。
4. **可测试性**：每个模块提供最小外观接口，测试通过公共 API 或契约接口完成。
5. **渐进迁移**：允许阶段性双轨（旧 API + 新 Facade）但必须在迭代计划中设置清理节点。

## 3. 分层视图
```
┌─────────────────────────────────────────────┐
│             Interface & Experience          │
│  Game · Sandbox GUI · Automation Tools      │
└──────────────▲──────────────────────────────┘
               │ Facade / DTO
┌──────────────┴──────────────────────────────┐
│            Runtime Services Layer           │
│  Scheduler · Snapshot · Event Bus · IO      │
└──────────────▲──────────────────────────────┘
               │ Domain API
┌──────────────┴──────────────────────────────┐
│            Simulation Kernel Layer          │
│  Simulation Loop · Agents · World Systems   │
└──────────────▲──────────────────────────────┘
               │ Foundation Contracts
┌──────────────┴──────────────────────────────┐
│            Foundation & Platform            │
│  Core Utils · Diagnostics · Resource I/O    │
└─────────────────────────────────────────────┘
```

## 4. 核心模块职责与目标命名

### 4.1 Foundation & Platform
- **命名空间 / 目标**：`genesis::base` → `Genesis::Base`；`genesis::diagnostics` → `Genesis::Diagnostics`。
- **职责**：
  - 公共类型、时间/文件工具、配置解析、线程工具。
  - 日志、统计、调试钩子（对接 spdlog / future telemetry sinks）。
  - 数据资产访问接口（抽象 `ResourceLocator`，供运行时与工具使用）。
- **依赖**：仅依赖标准库与第三方基础组件（spdlog、nlohmann_json 等）。

### 4.2 Simulation Kernel Layer
- **目标拆分**：
  - `Genesis::World`：世界结构、组件注册、持久化 DTO。
  - `Genesis::Agents`：需求系统、行动执行、规划器。
  - `Genesis::Simulation`：Tick 循环、调度器、系统编排（取代现有 `Engine.cpp` 多数逻辑）。
  - `Genesis::Telemetry`（可合并进 Simulation 初期）：仿真数据采集点。
- **职责**：
  - 保证世界状态演化、自洽性与系统间依赖顺序。
  - 通过稳定接口暴露 Tick、系统注册、状态查询能力。
- **依赖**：仅依赖 Foundation 层；`Agents` 和 `World` 被 `Simulation` 聚合。

### 4.3 Runtime Services Layer
- **目标拆分**：
  - `Genesis::Runtime::Core`：运行时上下文、事件队列、世界载入/保存、世界生成管线编排。
  - `Genesis::Runtime::Snapshot`：双缓冲快照、差分计算。
  - `Genesis::Runtime::Automation`（可选）：脚本化、批处理入口。
- **职责**：
  - 将 Simulation Kernel 封装为“可托管”进程，处理生命周期与资源管理。
  - 暴露只读/写命令接口，禁止直接触及 Simulation 内部对象。
  - 实现对外 DTO（快照、事件报告、世界生成结果）。
- **依赖**：Foundation + Simulation Kernel；禁止依赖 Interface 层。

### 4.4 Interface & Experience Layer
- **分区**：
  - `Genesis::Interface::SandboxGUI`：ImGui 前端，依赖 `Runtime::Core`/`Snapshot`，通过桥接器获取数据与发送命令。
- `Genesis::Interface::Game`：游戏客户端（运行时消费方，计划替代 CLI 作为主要交互入口）。
  - `Genesis::Tools::*`：资产编译器、调试工具；依赖 Foundation 与必要的 Runtime Facade。
- **职责**：
  - 呈现数据、收集用户指令、调试辅助。
  - 通过 Facade DTO 交互，禁止包含 Simulation 内部头文件。
- **依赖**：可以依赖 Runtime Services 与 Foundation；GUI 仅通过 Render/Style 子模块访问资产。

## 5. 依赖规则与契约
- **硬性规则**：
  - 接口层任何模块不得依赖 `src/simulation` 内部实现；仅使用头文件 Facade。
  - Runtime 层提供命令模型（`Command`, `Query`, `Event`），Simulation 层实现处理器；双方通过 DTO 传输。
  - 资产生成工具以 `Genesis::Assets` 提供的服务为入口，避免直接操作文件路径常量。
- **建议依赖路径**：
  - `Genesis::Style` 改为依赖 `Genesis::Assets::Palette`（新 Facade），而非直接读 JSON。
  - 测试使用 `Genesis::Runtime::TestHarness`（轻量适配器）构造模拟环境，减少对具体实现的侵入。

## 6. 构建系统规划（CMake）
- 新增目标示意：
  - Foundation：`genesis_base`、`genesis_diagnostics`、`genesis_assets`。
  - Simulation：`genesis_world`、`genesis_agents`、`genesis_simulation`、`genesis_telemetry`.
  - Runtime：`genesis_runtime_core`、`genesis_runtime_snapshot`、`genesis_runtime_automation`.
- Interface/Tools：`genesis_game`（计划）、`genesis_sandbox_gui`, `genesis_palette_compiler`, `genesis_debug_console`。
- 约束：
  - 通过 INTERFACE 库聚合公共头文件，减少 include 路径膨胀。
  - 资产生成目标补充 `BYPRODUCTS`、`OUTPUT` 与目录创建命令，保证多生成器一致性。
  - 单测按模块拆分，如 `genesis_simulation_tests`、`genesis_runtime_tests` 等，避免单一可执行承载全部领域。

## 7. 迁移路线图（建议）
1. **Phase 0：基础设施**
   - 引入 `Genesis::Base` 与 `Genesis::Diagnostics`，迁移通用工具。
   - 调整现有目标 include path，使 `Engine` 使用新基础层。
2. **Phase 1：Simulation Kernel 拆分**
   - 将世界、代理、系统逻辑移动至独立库；保留旧 `Genesis::Engine` 作为薄 Facade。
   - 重写单测以适配新模块。
3. **Phase 2：Runtime Facade 重建**
   - 拆出 `Runtime::Core` + `Runtime::Snapshot`，提供命令/事件 API。
  - GUI/Game 改用 Facade；移除对 `Engine` 的直接引用。
4. **Phase 3：Interface 层调优**
   - 将 `RuntimeBridge.cpp`、`AppHostCore.cpp` 拆分成 Presenter/Command/Query 子组件。
   - 引入 UI 状态容器与渲染适配器，减小单文件体积。
5. **Phase 4：工具与资产流水线**
   - 统一资产服务，完善 palette/世界生成工具的脚本化接口。
   - 评估远程调试/自动化接口的扩展点。
6. **Phase 5：收尾**
   - 清理旧 API，更新所有文档与示例。
   - 检查二进制/包发布脚本，确保新模块结构可被消费。

## 8. 风险与预研议题
- **跨平台构建**：Runtime DLL 复制策略需要在 macOS/Linux 上验证；可能需要 `RPATH` 或启动脚本。
- **性能回归**：拆分模块可能带来额外接口层；需要在 Phase 2 后进行性能基准测试。
- **世界资产兼容性**：重写 Facade 时注意保持序列化格式不变，避免破坏既有数据。
- **测试迁移成本**：需要同步重写测试夹具，可考虑引入 `TestHarness` 统一管理引擎生命周期。

---
后续步骤：基于本蓝图制定具体子任务（每个 Phase 细化为 Issues/PR），并在实施过程中持续回填文档与经验。***

## 迁移进展（2025-10-23）

- Phase 0：基础设施（已落地最小骨架）
  - 新增 `Genesis::Base` 与 `Genesis::Diagnostics`（头文件级封装），作为公共类型与日志门面占位，后续逐步替换直接使用 spdlog 的调用。
- Phase 1：仿真内核解耦（第一步）
  - 降低 GUI 对内核的头文件耦合：`src/apps/sandbox_gui/include/sandbox/gui/RuntimeBridge.hpp` 移除对 `genesis/core/Engine.hpp` 的直接包含，改为前置声明；`RuntimeBridge.cpp` 内部包含实现依赖。
  - CMake 实体化 `Genesis::World` 静态库（迁移 `world/system/ResourceSystem`），并让 `Genesis::Simulation` 通过公共接口依赖世界模块。
- Phase 2：运行时子域（占位）
  - 引入 `SimulationService` Facade 与默认实现 `EngineSimulationService`，Runtime 可通过工厂替换底层仿真服务；`Genesis::RuntimeCore`、`Genesis::RuntimeSnapshot` 仍指向现有动态库，占位待拆分。

影响与兼容性：本次变更不影响现有可执行与功能，编译链保持稳定；GUI 侧头文件边界更清晰，为“界面层不依赖仿真实现”打基础。

## 破坏性重构计划 · v2（Map 图 + 直线移动）

> 结论复述：彻底切换到“Map 图（有向）+ Scene 分组 + Interaction/Portal”的世界模型；运行时 Map 内直线移动；渲染层仅在“可见范围内”执行局部寻路用于可视化。此为破坏性重构，不保留旧 `LocationGraph`/`WorldRegistry`/基于节点边的寻路兼容层。

### Phase A · 运行时与世界模型（全面替换）
1) 数据模型与查询
- 新类型：`Map/MapEdge`、`Scene`、`Interaction{ id,mapId,sceneId,kind,coord }`、`Portal{ interactionId,... }`。
- 新总线：`WorldDatabase`（替代 `WorldRegistry`），提供：`maps()`、`mapEdges(from)`、`scenes(mapId)`、`interactions(mapId)`、`portals(mapId)`、`findInteraction(id)` 等只读查询。
- 移除：`LocationGraph/LocationNode/PathEdge` 与对应查询接口。

2) ECS 组件
- `AgentLocation { mapId:uint32, position:{x,y}, atInteractionId?:uint32 }`
- `MovementIntent/State { targetMapId:uint32, targetPos:{x,y}, ... }`（去除“节点路径”）
- `ActionTask { targetMapId, targetPos }`；资源消费以 `interactionId` 标识目标。

3) 系统
- `MovementSystem`：Map 内直线运动（欧氏距离/速度），到点即达；跨图跳转后续在 MapEdge 层拼接。
- `ResourceSystem`：库存绑定到 `Interaction(kind=Resource)`；按 interactionId 消耗/再生。
- `Planner`（示例：觅食）：在同一 Map 内对 Resource interaction 打分/选择并生成行动（暂不跨图）。

4) Engine/Telemetry
- Engine 使用 `WorldDatabase` 查询；Demo 代理在某 map 的某交互点或坐标生成。
- Telemetry 输出统一为 `{ mapId, position(x,y) }` 与可选 `movement{from,to,t01}`；移除对 `LocationId` 的依赖。

5) Loader/存取
- 替换为：`world.json`（`maps[]`、`map_edges[]`）+ `map_{id}.json`（`scenes[]`、`interactions[]`、`portals[]`、可选 `tilemap`）。
- 实现完整读取与基本校验；移除旧 JSON（`LocationGraph`）路径与 Loader。

6) Atlas/GUI 基座
- `RuntimeBridge::buildWorldAtlas` 改为由 `WorldDatabase` 构造：`maps/mapEdges` + 每图 `scenes/interactions/portals[/tilemap]`。
- GUI：
  - MapView：绘制 Map 图与交互点；
  - SceneView：按 Tile 层渲染并高亮交互点/Portal；
  - Inspector：以 Interaction/Portal 为一等公民。

验收（A）：工程可构建；Demo 代理在单图内直线运动；资源消费闭环正常。GUI 适配可延后到 Phase A3。

构建开关（便于专注内核重构）：
- 在 `src/CMakeLists.txt` 新增选项：
  - `-DGENESIS_BUILD_GUI=OFF`（默认 OFF）禁用 Sandbox GUI 构建。
  - 仅编译内核与运行时：`cmake -S src -B build -DGENESIS_BUILD_GUI=OFF`。

### Phase B · 渲染层“可见内”局部寻路（仅可视化）
1) 可见性与资产
- SceneView 按相机窗口判断可见；可见 Map 载入 Tile 纹理/元数据。

2) 局部寻路（GUI 自行计算）
- LOS 直线优先：对起终可视点做网格/采样直线检测，可通行则用直线渲染。
- 失败回退 A*：在小范围（视口内）做轻量 A*，生成 polyline；结果按 `agentId+segment` 缓存；越界/离屏失效即丢弃。
- 渲染沿 polyline 插值；此路径不改变运行状态。

3) 与运行解耦
- 运行时仍按直线/MapEdge 推进；GUI 仅为“看到的对象”计算/缓存可视路径，避免全局成本。

验收（B）：可见范围内，代理以平滑路径渲染；离开视口后停止计算；切换 Map/世界版本时缓存正确刷新。

### 非兼容声明与迁移
- 本次为破坏性重构：移除 `LocationGraph`、基于节点/边的 Movement/Planner/Loader/Atlas，API 同步更改。
- 旧数据需迁移为 `world.json + map_{id}.json`；提供最小示例与校验脚本。

### 风险与里程碑
- 风险：大规模签名变更影响范围广；GUI 可视寻路的缓存与性能需验收；测试夹具需重写。
- 里程碑：
  1) A1 数据模型 + WorldDatabase + Loader 完成；
  2) A2 Movement/Resource/Planner + Telemetry 对齐；
  3) A3 Atlas/GUI 适配；
  4) B1 可视范围局部寻路（LOS + A*）与缓存；
  5) B2 性能与 UX 打磨；
  6) 收尾清理旧引用与文档。
