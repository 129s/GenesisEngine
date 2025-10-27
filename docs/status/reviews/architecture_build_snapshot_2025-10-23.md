# 架构与构建现状快照（2025-10-23）

> 目的：在大规模清扫前记录当前工程的架构与构建状态，方便后续拆解与跟踪。

## 工程与依赖划分
- CMake 3.21 起步，全部目标集中在 `src/CMakeLists.txt`；通过 `cmake/CPM.cmake` 下的 CPM 拉取第三方库（spdlog、EnTT、nlohmann_json、tomlplusplus、GLFW、Dear ImGui docking 分支），并要求系统级 Freetype。
- 静态库模块：
  - `Genesis::PaletteData`：封装调色板 JSON 解析逻辑，依赖 `nlohmann_json`+`spdlog`，为风格模块生成原始数据。
  - `Genesis::Style`：消费调色板生成的头文件，引入 `generated` 目录；对外只暴露 Palette API，自身不直接依赖 GUI。
  - `Genesis::Rendering`：容纳渲染资产缓存/打包逻辑，目前不直接依赖图形 API（GL 绑定发生在上层 GUI）。
  - `Genesis::Worldgen`：处理世界生成模块，唯一外部依赖为 `tomlplusplus`。
  - `Genesis::Engine`：聚合 agents/world/planner/telemetry 等多个子域；目前为单一静态库，缺少更细颗粒度的拆分。
- 共享库 `Genesis::Runtime`：对外提供运行时快照生成、世界生成、事件队列等功能；内部私有链接 `Genesis::Engine`，公有链接 `Genesis::Worldgen`，并向消费者暴露 `Engine` 引用（增大与核心库的耦合）。
- 自定义目标：
  - `genesis_palette_compiler` 可执行程序负责将 `data/palette/core.json` 转为 C++/GLSL 资产。
  - `genesis_palette_assets` 自定义目标驱动资产生成，但未显式声明输出目录创建或 BYPRODUCTS，依赖工具内部行为。

## 可执行产物
- `genesis-engine`：命令行烟雾测试入口，只依赖 `Genesis::Engine`。
- `genesis-sandbox-gui`：主 GUI，链接 `Genesis::Style`、`Genesis::Runtime`、`Genesis::Rendering`、ImGui、GLFW，以及平台相关的 OpenGL 库；GUI 代码集中在 `src/apps/sandbox_gui/gui`，其中 `RuntimeBridge.cpp` (~33k 行) 与 `AppHostCore.cpp` (~22k 行) 体量巨大，提示高耦合。
- `genesis_palette_compiler`：构建期/测试期资产生成工具，同时被部分 CTest 项直接调用。

## 目录与模块边界
- `src/` 与 `include/` 按功能域镜像展开：`agents/`、`world/`、`worldgen/`、`runtime/`、`style/` 等。
- 核心业务逻辑基本集中在 `Genesis::Engine` 下，`include/genesis/core`、`include/genesis/world`、`include/genesis/agents` 暴露宽接口，缺乏子模块装配层（例如世界系统与 AI 系统之间的交互全部在 `Engine.cpp` 聚合）。
- GUI 模块按照 MVC-ish 结构划分 controller/presenter/ui/style，但目前仍通过直接包含 `src` 下实现文件在测试中重用，说明缺乏清晰的对外 API。
- `runtime` 层既负责与 GUI 对接（快照、事件）又负责世界生成与文件 IO，承担大量跨域职责，后续需要拆分为运行时内核与桥接层。

## 构建与测试流水线
- 默认打开 `GENESISENGINE_ENABLE_TESTS`，生成 5 个 GTest 可执行文件（Style、Worldgen、Engine、Runtime、Sandbox GUI）以及两个额外的 `ctest` 入口（`GenesisRuntime_Smoke`、`GenesisEngine_E2E`）。
- 运行时测试通过 `add_custom_command` 在构建后复制 `genesis_runtime` DLL 至测试目录，并通过修改 `PATH` 执行；非 Windows 平台目前未设置 `LD_LIBRARY_PATH` 等变量，存在跨平台风险。
- 调色板生成流程被多个地方依赖：`Genesis::Style` 构建依赖 `genesis_palette_assets`，测试中再次调用工具输出到 `build/tests/palette_artifacts`，说明资产生成逻辑在构建与测试阶段均为硬性前置条件。

## 主要风险与改进切入点
- **大型单体实现文件**：`RuntimeBridge.cpp`、`AppHostCore.cpp` 等超过 20k 行，后续重构需优先拆分成子组件以降低修改成本。
- **引擎库聚合过多领域**：目前 `Genesis::Engine` 将 agents/world/planner/telemetry 全部打包到同一目标，缺少层次化模块，给依赖者提供“全量”接口；建议评估拆分成 `Genesis::Simulation`、`Genesis::Agents` 等子库。
- **资产生成目标的健壮性**：`genesis_palette_assets` 没有保证输出目录存在或声明 BYPRODUCTS，多配置生成器（VS）下可能重复触发或找不到生成物。
- **测试/构建耦合**：GUI 测试直接纳入生产代码实现文件，多点生成资产，加大了回归成本；可考虑提炼最小接口或引入轻量 Mock。
- **运行时依赖暴露**：`Runtime` 头文件直接暴露 `Engine` 引用，使得调用方可以绕过运行时 API 操作核心引擎，后续清理需要明确边界。

## 建议的后续动作（供讨论）
1. 设计目标模块图，确认 Engine/Runtime/GUI 的分层关系与拆分顺序。
2. 为构建期资产生成补充目录创建、BYPRODUCTS 声明，并梳理测试对生成资产的依赖方式。
3. 审核 GUI 层大文件，先行拆出渲染、命令处理、数据同步等子组件。
4. 评估 `Runtime` 公有 API，与 Engine 的直接暴露部分是否可以通过 Facade/Command 模式替换。
