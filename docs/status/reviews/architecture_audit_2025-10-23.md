# 架构评估 · 2025-10-23

## 概览
- 聚焦 GenesisEngine 当前命名惯例、目录划分、技术债务与世界数据管线的双轨问题。
- 列出的风险按照影响度排序，供后续规划重构与文档更新。

## 命名与命名空间
- 引擎核心沿用 `namespace genesis::…` 小写形式，例如 `include/genesis/core/Engine.hpp:24`，而 Sandbox GUI 与 Style 子系统则使用 `namespace Genesis::…`（诸如 `src/apps/sandbox_gui/include/sandbox/gui/AppHost.hpp:27`、`src/engine/style/PaletteLoader.cpp:14`）。混用大小写命名空间会让调用方需要反复切换别名，统一方案尚未落地。
- `include/genesis/worldgen/Types.hpp:15` 再次使用小写 `genesis::worldgen`，强化了「库内使用小写 / 新增子系统使用首字母大写」的割裂感。需要决策：要么整体迁回小写，要么为旧模块建立别名，避免新旧 API 并存两套命名。

## 目录结构与目标划分
- `src/apps/sandbox_gui/Main.cpp:1` 与 `src/apps/sandbox_gui/gui/**` 一同编译为 `genesis_sandbox_gui` 可执行文件，导致应用入口与框架代码混在一个目标里，后续若要引入第二个 GUI 入口难以拆分。
- 测试侧缺少可复用库：`tests/CMakeLists.txt:75-88` 直接把 `../src/apps/sandbox_gui/gui/presenter/MainViewPresenters.cpp` 拉进测试目标并手工暴露 `include` 与 `src` 目录，说明 Sandbox GUI 尚未抽象为可链接库。
- GUI 目录下存在仅在实现中引用却放在 `src` 的头文件（如 `src/apps/sandbox_gui/gui/CommandUiHelpers.hpp:1`、`src/apps/sandbox_gui/gui/FilesystemHelpers.hpp:1`），会诱导外部模块跨目录 include，破坏封装。

## 技术债务热点
- `src/apps/sandbox_gui/gui/ui/MainView.cpp` 长达 2103 行，`drawSceneUnified`（1487 起）与 `drawSceneViewport`（1629 起）承担状态采集、交互处理与绘制逻辑的全部职责，缺乏可单测的拆分点。
- `src/apps/sandbox_gui/gui/RuntimeBridge.cpp` 约 1000 行，既负责后台线程生命周期、快照 Ring Buffer，也内联世界拓扑投影（`buildWorldAtlas`）与命令队列状态机，出现“上层驱动 + 领域转换”混杂。
- `src/apps/sandbox_gui/gui/AppHostCore.cpp`（551 行）仍集中了窗口初始化、渲染循环、输入分发与命令快捷键注册，尚未引入更细粒度的控制器/服务组件。
- 以上文件未提供清晰的模块边界，导致 Sandbox GUI 重构需要一次性修改巨型 TU，测试粒度也只能停留在端到端。

## 世界数据管线的双轨现象
- GUI 侧的 `RuntimeBridge::buildWorldAtlas`（`src/apps/sandbox_gui/gui/RuntimeBridge.cpp:966-984`）假设所有节点具备 `coord_global`，否则直接返回空图并打印错误；默认演示数据 `data/world/generated/noise_mvp.json` 未携带此字段（`rg` 搜索为空），运行时将无法渲染 Scene 画布。
- 文档层面已宣布噪声地图管线退役（`docs/status/backlog/noise-map-mvp.md:3`），却仍在运行时测试中引用该 JSON（`tests/test_runtime.cpp:35/53/64/119`），显示旧数据与新管线并存。
- Schema 迁移说明（`docs/handbook/WORLD_SCHEMA_MIGRATION.md:6-28`）与待办列表（`docs/status/todo.md:20`）都指出 `coord_local/coord_global` 补全尚未完成；Sandbox GUI 既要兼容旧世界，又要消费新字段，显露双轨兼容状态未结束。

## 其他观察
- `src/apps/sandbox_gui/gui/style/DesignTokens.cpp:20-92` 内建整套 fallback 色板，与 `Genesis::Style::ColorRegistry`（`src/engine/style/PaletteLoader.cpp`）的调色板生成器并行存在；Palette 生成失败时才退回旧色板，表明样式系统正处于迁移中。
- `genesis_runtime` 被构建成动态库（`src/CMakeLists.txt:174`）并在测试阶段手工复制至运行目录（`tests/CMakeLists.txt:96`），静态库与可执行混搭，会对后续部署脚本与包体设计提出额外要求。

## 建议
- 统一命名空间策略，提前规划重命名步骤（例如引入 `namespace genesis::sandbox` 别名，再逐步迁移 GUI 与 Style）。
- 将 Sandbox GUI 抽成 `Genesis::SandboxGui` 静态/动态库，`src/apps` 只保留入口层；测试改为链接该库，避免直接引用实现文件。
- 为 `RuntimeBridge`、`MainView` 引入 Presenter/View/Service 三级拆分，积累可单元测试的组件，并减小 TU。
- 在世界数据层补全 `coord_global` 迁移工具，或为 GUI 引入兼容模式（缺失坐标时自动降级布局），尽快关闭旧噪声数据依赖。
