# GenesisEngine 构建流程总览

本文档汇总当前主干分支的构建、测试与生成资产的工作流，适用于 Windows/Linux/macOS 开发环境。若后续流程调整，请同步更新此文档并在 PR 中注明。

## 1. 环境与依赖准备

注意：v2 使用 WorldDatabase（`world.json + map_{id}.json`）作为数据通道；GUI 与样式系统默认关闭，可按需启用。
- CMake ≥ 3.21（推荐与 Ninja 一起使用，加快增量构建）
- 支持 C++20 的编译器：MSVC 17 系列、Clang 14+ 或 GCC 11+
- （可选）Freetype：仅在启用 GUI 且希望 ImGui 使用 Freetype 渲染时需要；未安装也可正常构建（GUI 会退化为默认字体渲染路径）
- GPU/桌面环境：仅运行 `genesis_sandbox_gui` 需要可用的 OpenGL 上下文

项目依赖通过 `cmake/CPM.cmake` 自动拉取，包含 spdlog、EnTT、nlohmann_json、tomlplusplus、GLFW、Dear ImGui（docking 分支）。首次配置会下载并编译这些第三方库。

## 2. 配置（Configure）
建议统一使用 out-of-source 构建目录：

```bash
cmake -S . -B build -G Ninja ^
  -DGENESISENGINE_ENABLE_TESTS=ON ^
  -DCMAKE_BUILD_TYPE=Debug
```

> Windows PowerShell 下可改用反斜杠续行；若使用 MSVC 默认生成器，可去掉 `-G Ninja`。

常用 CMake 选项：
- `GENESISENGINE_ENABLE_TESTS`（默认 ON）：控制是否生成所有 GTest 目标
- `CMAKE_BUILD_TYPE`（单配置生成器时有效）：`Debug` / `RelWithDebInfo` / `Release`
- `CMAKE_TOOLCHAIN_FILE`：如果通过 vcpkg 提供 Freetype，可指定 `vcpkg.cmake`

## 3. 编译（Build）
基础命令：

```bash
cmake --build build               # 全量构建
cmake --build build --target genesis_sandbox_gui
cmake --build build --target genesis_runtime_cli
```

主要产物位于 `build/src`：
- `genesis-sandbox-gui`：ImGui 驱动的沙盒 GUI，可交互查看世界状态
- `genesis-runtime-cli`：Headless 工具，可执行 JSON 命令脚本（用于回归/重放）
- `genesis_runtime`（动态库）、`genesis_worldgen` / `genesis_world_model` / `genesis_simulation` 等（库目标）
- Game 客户端（计划）：作为最终用户入口，构建目标与运行脚本将在后续迭代提供

## 4. 测试与验证
启用测试选项后，CMake 将注册以下目标：
- `genesis_worldgen_tests`
- `genesis_runtime_tests`
- `genesis_world_model_tests`
- `genesis_world_tests`
- （启用 GUI 时）`genesis_sandbox_gui_tests`

执行 GTest 套件与端到端检查：

```bash
cmake --build build --target genesis_runtime_tests
ctest --test-dir build --output-on-failure
```

`ctest` 将自动运行已注册的 gtest 二进制及若干 CTest 条目（以 `tests/CMakeLists.txt` 为准）。
其中包含一条 Runtime CLI 回放用例（`RuntimeCli.RunWorldCycleScript`），用于覆盖 “脚本→命令队列→执行→世界保存” 的最小闭环。

## 5. 世界生成（Worldgen）
- Worldgen 可独立运行（通过 Runtime 命令 `world.db.generate` 或 headless 脚本），不再依赖旧的 `LocationGraph`。
- 设计与数据产物约定：见 `docs/architecture/world/world-generation.md`、`docs/handbook/WORLD_SCHEMA_MIGRATION.md`。

## 6. 增量构建与常见目录
- 源码：`src/`（核心库、工具、GUI）、`include/`（公共头文件）
- 构建输出：`build/src`（可执行/动态库）、`build/lib`（静态库）、`build/tests`（测试二进制）
- 测试资产：`tests/` 与 `build/telemetry_*.json`
- 重新配置：`cmake -S . -B build` 会复用现有缓存；必要时删除 `build/CMakeCache.txt` 或重建新目录

## 7. 推荐工作流
1. 首次配置：`cmake -S . -B build -G Ninja`
2. 编译关键目标：`cmake --build build --target genesis_sandbox_gui`
3. 运行测试：`ctest --test-dir build --output-on-failure`
4. （可选）通过沙盒 GUI 的 World Generation 面板或运行时命令脚本验证世界生成流程
5. 在 `build/src/genesis-sandbox-gui` 目录下启动 GUI 进行回归验证

Windows 一键回归（Core only）：`powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run_regression_core.ps1 -Config Debug -BuildDir build_core`

若引入新依赖或修改工具链，请记得更新本文件并在 PR 描述中说明。

