# 调色板模块（Palette Service）设计与使用说明

本页记录 `Genesis Engine` 新增的通用调色板模块，用于统一 PPA、Runtime、Render、Game 以及 Sandbox GUI 的颜色来源，减少魔法数并为未来的多主题/动态主题能力打基础。

## 目标与能力概览
- 单一数据源：`data/palette/core.json` 描述核心色板，可按主题扩展。
- 语义化令牌：颜色通过语义名称（如 `surface.canvas`、`ppa.sky.zenith`）检索，支持跨层共享。
- 运行时接口：`Genesis::Style::ColorRegistry` 提供加载、主题切换、颜色空间转换（sRGB/Linear）等 API。
- 兼容旧代码：Sandbox GUI 仍可按旧枚举访问颜色，底层自动映射到调色板。
- 可测：新增 `tests/test_palette.cpp` 验证色板加载、别名解析与线性色空间转换。

## 数据布局
```
data/
  palette/
    core.json        # 默认主题（genesis/core-dark），后续可并列新增其他主题文件
```

`core.json` 结构（节选）：
```jsonc
{
  "version": 1,
  "default_theme": "genesis/core-dark",
  "themes": [
    {
      "id": "genesis/core-dark",
      "label": "Genesis Core Dark",
      "tokens": {
        "surface.canvas": {
          "category": "surface",
          "description": "三维视图和 dockspace 的最底层画布背景。",
          "srgb": "#191D21",
          "aliases": ["Canvas", "ui.canvas"]
        },
        "accent.primary.base": { "srgb": "#32C3FF", "aliases": ["Primary"] },
        "ppa.sky.zenith":     { "srgb": "#1B2E59" },
        "...": {}
      }
    }
  ]
}
```

说明：
- `srgb` 可以是字符串（`#RRGGBB[AA]`）、数组 `[r,g,b,a]` 或对象 `{r,g,b,a}`；`linear`（可选）同理。当未提供 `linear` 时自动由 sRGB 计算。
- `aliases` 可选，用于兼容旧命名或提供别名检索。
- `category`、`description` 供文档与调色工具使用，运行时代码可忽略。

## 运行时接口
头文件：`include/genesis/style/ColorRegistry.hpp`

核心方法：
- `ColorRegistry::instance()` 获取全局单例。
- `bool tryLoadFrom(std::filesystem::path, std::optional<std::string>)` 加载调色板并可选指定激活主题。
- `std::optional<RgbaColor> color(token, ColorSpace, theme)` 获取颜色（默认 sRGB，支持线性空间）。
- `bool setActiveTheme(std::string_view)` 切换主题。
- `std::vector<std::string> themes()/tokens()` 枚举可用主题、令牌。

Sandbox GUI 的 `DesignTokens` 已改为优先查询 `ColorRegistry`，若色板缺失会退回旧的硬编码表并输出 warn。

> **注意**：库默认尝试从当前工作目录向上查找 `data/palette/core.json`。若可执行文件放置在其他路径，需在初始化阶段调用 `ColorRegistry::instance().tryLoadFrom()` 明确指定配置位置。

## 离线调色板编译工具
路径：`src/tools/palette_compiler`，可执行文件名：`genesis_palette_compiler`

用途：在构建阶段将 JSON 色板转换为多种产物，供不同子系统直接使用。

### 运行方式
- 推荐：`cmake --build build --target genesis_palette_assets`（自定义目标会在 `build/generated/palette/` 下产出所有文件）
- 手动：`build/src/genesis_palette_compiler --input data/palette/core.json --output <out-dir> [--theme <id>] [--skip-cpp|--skip-glsl|--skip-csv]`
  - `--theme` 可多次指定，仅生成所需主题的产物
  - `--quiet` 降低日志输出

### 当前输出
- `PaletteGenerated.hpp`：包含 `Genesis::Style::Generated` 命名空间内的 `ThemeInfo`、`PaletteEntry` 常量数组，可在 C++ 侧直接引用。
- `PaletteGenerated.glsl`：为每个主题生成常量宏，方便渲染管线 include。
- `palette_<theme>.csv`：面向 PPA/工具链的 CSV 导出（含 srgb/linear 数值与语义信息）。
- `PaletteIndex.json`：汇总版本、默认主题与各类产物路径，便于后续工具消费。

测试：`ctest` 目标中新增 `PaletteCompiler.GenerateArtifacts`，确保工具可正确执行。

## 命名约定
- 语义格式：`<domain>.<sub-domain>[.<variant>]`，避免直接出现组件名或颜色描述（如 `blue`）。
- 常用 domain：`surface`、`text`、`status`、`accent`、`render`、`ppa.sky`、`ppa.biome` 等。
- variant 例子：`*.base`、`*.hover`、`*.active`、`*.muted`。
- 尽量提供 `category` 描述真实用途，方便 PPA/美术或 UI 设计沟通。

## 贡献流程
1. 修改或新增调色板 JSON。
2. 如引入新令牌，更新相应代码（或映射表）并补充测试。
3. 运行 `ctest` 或至少执行 `genesis_style_tests`，确保加载和色值计算无回归。
4. 更新本页或其他相关文档，记录新增令牌及用途。
5. 提交变更（建议 `git commit -am "..."` 前确保工作区干净）。

## 后续规划建议
- **多主题管理**：在 JSON 中追加 `light`、`high-contrast` 等主题，运行时按配置切换。
- **校验脚本**：加入别名冲突检测、对比度检测（WCAG）、色域检查等自动化流程。
- **动态调色**：扩展 JSON 支持随机采样范围、渐变序列，为 PPA/渲染算法提供更多素材。

如需新增主题或扩展 schema，请先在 Issue/文档中同步设计，避免破坏现有使用方。
