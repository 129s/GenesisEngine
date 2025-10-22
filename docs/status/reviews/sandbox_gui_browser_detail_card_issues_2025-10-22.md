# Sandbox GUI：Browser 详情卡片问题分析（2025-10-22）

本分析基于实际截图与当前实现（`src/sandbox/gui/ui/BrowserView.cpp`）。问题集中在详情卡片的信息表达与平台一致性。

## 复现
- 打开 Sandbox GUI → Browser 面板。
- 选中 `data/` 根目录（默认选中）。
- 观察右侧“详情”卡片：
  - `相对路径` 显示为 `.`。
  - `绝对路径` 在窄面板下被自动换行，并使用正斜杠 `/` 分隔。

## 主要问题与表现
- 相对路径显示不友好：根目录显示为 `.`，对非技术用户不直观。
- 绝对路径断行影响可读/可复制：中间断开如 `Genesis\nEngine/...`。
- Windows 上分隔符不符合习惯：展示为 `/`，复制到资源管理器不够顺手。
- 缺少快捷操作：无「复制路径」「在文件管理器中打开」等常用入口。
- 目录额外信息不足：未展示目录内项目数，无法快速判断内容规模。
- 标点/文案一致性风险：历史版本出现过中/英冒号混用的情况（截图可见），需统一。

## 根因定位（源码）
- 相对路径为点：`relativeKey(...)` 对根返回 `"."`，并直接用于展示。
  - 参见：`src/sandbox/gui/ui/BrowserView.cpp:47`（函数定义），`src/sandbox/gui/ui/BrowserView.cpp:424`（渲染相对路径）。
- 绝对路径自动换行：详情卡片在进入时设置了 `PushTextWrapPos(...)`，且绝对路径使用 `TextWrapped` 输出。
  - 参见：`src/sandbox/gui/ui/BrowserView.cpp:382`（设置 wrap），`src/sandbox/gui/ui/BrowserView.cpp:425`（绝对路径 `TextWrapped`）。
- 正斜杠来源：调用 `selectedPath.generic_string()` 强制使用 `/`。
  - 参见：`src/sandbox/gui/ui/BrowserView.cpp:425`。

## 建议改进（最小改动优先）
- 相对路径：根目录显示为 `"/"` 或 `"(根) data/"`，或文案改为 `位置` 并特殊处理 `"."`。
- 绝对路径：
  - 展示使用本地分隔符：`selectedPath.lexically_normal().make_preferred().string()`；
  - 避免断行：该行使用 `TextUnformatted` 并在输出前临时 `PushTextWrapPos(FLT_MAX)`；或用 `InputText(ReadOnly|AutoSelectAll)` 提供可选中、单行显示；
  - 追加操作：同行放置 `复制` 按钮（`ImGui::SetClipboardText`）与 `打开`（Windows 执行 `explorer /select, <path>`；跨平台按 `xdg-open`/`open` 区分）。
- 目录信息：目录时显示 `包含：N 项`（忽略不可访问条目），错误时显示警告。
- 文案一致性：统一使用中文全角冒号（当前实现已基本统一，但需回归自查）。

## 参考代码位置
- 详情卡片入口：`src/sandbox/gui/ui/BrowserView.cpp:363`
- 相对/绝对路径渲染：`src/sandbox/gui/ui/BrowserView.cpp:424`、`src/sandbox/gui/ui/BrowserView.cpp:425`

## 后续工作（如采纳）
- UI 变更：按上述建议微调 4 处代码，新增复制/打开两个按钮（不引入新依赖）。
- 交互验证：窄/宽面板下路径展示与操作是否顺手；Windows 与 Linux/Mac 分隔符是否正确。
- 文档与截图：更新 `docs/guides/sandbox_gui_smoke.md` 的使用截图与说明。
