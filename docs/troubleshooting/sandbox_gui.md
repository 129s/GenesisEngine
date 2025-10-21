# Sandbox GUI 故障排查

本文汇总近期 `genesis_sandbox_gui` 相关已知问题与修复建议。

## 2025-10-17 · 启动后部分环境未响应 / 约第 200 步闪退

症状：
- 在部分终端（交互式 CLI）直接运行 GUI 时仅显示 Welcome 面板且程序“未响应”。
- 在资源充足的本地环境中双击或在其它终端运行时，约第 120~201 步附近崩溃（SIGSEGV / 断言失败）。

根因：
- 引入“代理位置插值”后，`RuntimeBridge::resolveAgentPosition` 在 GUI 后台线程读取了 ECS (`entt::registry`) 中的 `MovementState`，该访问与模拟线程并发，存在数据竞争与生命周期问题，触发 `std::vector<MovementState*>` 越界断言或段错误。

修复：
- 移除 GUI 线程对 `registry` 的直接读取，改为仅依据 `TickTelemetry` 的离散 `LocationId` 通过 `WorldAtlas` 映射静态坐标渲染。
- 变更点：
  - 删除 `RuntimeBridge::resolveAgentPosition`，在 `captureSnapshot` 中不再查询 ECS，改为 `atlas.nodePosition(agent.location)`。
  - 后续若需平滑插值，应在 Runtime 侧产出稳定的插值所需数据（如 segment 进度）并写入 `Telemetry`，由 GUI 只读消费。

验证：
- 本地 Debug/Release 均可持续运行多轮（> 3600 步）无崩溃，CLI 环境运行亦不再卡死。

注意：
- Windows 远程/无图形会话可能无法创建 OpenGL 上下文，仍建议在有桌面/驱动的环境运行，详见 `docs/guides/sandbox_gui_smoke.md`。

## 2025-10-17 · 新数据契约下渲染缺失/错位

症状：
- Map View 不显示部分节点或位置错位；Scene View 无法渲染代理入/出场或资源点不见。

根因（世界数据未满足新契约）：
- `locations[i].coord_global` 缺失 → Map 无法定位节点。
- `edges[i].anchors` 缺失或不合法 → Scene 入/出场锚点无参照。
- `spawns[i].local_coord` 缺失 → 资源/交互点无局部坐标。
- `schema_version` 不匹配 → 前端按旧协议解析导致字段缺省。

排查与修复：
1) 检查世界 JSON 是否包含上述字段（参见 `docs/architecture/WORLD_MODEL.md` 与迁移指南）。
2) 确认 `schema_version` 与 Runtime/GUI 约定一致。
3) 使用最小示例数据验证渲染（来自迁移指南的片段）。
4) Loader 报错：自 2025-10 起，WorldLoader 将强校验上述必备字段，缺失会返回错误并拒绝加载（不再提供旧版回退路径）。

## 2025-10-21 · 锚点标签 “锚/拽” 渲染成 “?”

症状：
- Sandbox 场景内锚点标注中文字 “锚”、“拽” 显示为问号，其余中文字符正常。

根因：
- `AppHost::initializeImGui` 仅通过 `io.Fonts->GetGlyphRangesChineseSimplifiedCommon()` 将系统 CJK 字体合并至 ImGui 字体图集。
- 该 2500 常用字范围不含 U+951A（锚）与 U+62FD（拽），导致 Freetype 构建图集时丢弃对应字形，呈现为 fallback `'?'`。

排查要点：
1) 复现后抓取 `imgui_draw.cpp` 中 `accumulative_offsets_from_0x4E00[]` 并验证目标编码不在常用字列表内，例如：

```powershell
$content = Get-Content -Path 'build/_deps/imgui-src/imgui_draw.cpp' -Raw
$token = 'accumulative_offsets_from_0x4E00[] ='
$start = $content.IndexOf($token)
$braceStart = $content.IndexOf('{', $start)
$braceEnd = $content.IndexOf('};', $braceStart)
$body = $content.Substring($braceStart + 1, $braceEnd - $braceStart - 1)
$nums = $body -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ }
$base = 0x4E00
$total = 0
$codepoints = @()
foreach ($n in $nums) { $total += [int]$n; $codepoints += $base + $total }
'锚' + ' in range? ' + ($codepoints -contains 0x951A)
'拽' + ' in range? ' + ($codepoints -contains 0x62FD)
```
2) 确认 `data/fonts` 未提供自定义字体时，实际加载的是 `C:\Windows\Fonts\simsun.ttc` 等系统字体，本身包含所需字形，排除字体文件缺失的可能。

修复建议：
- 改用 `io.Fonts->GetGlyphRangesChineseSimplifiedCommon()` + 自行追加稀用字（`ImFontGlyphRangesBuilder::AddText("锚拽")`），或直接采用 `GetGlyphRangesChineseFull()`。
- 若关注 atlas 体积，可仅在场景锚点模块按需追加关键字形并缓存生成的 `fonts/.cache`，避免每次启动重新构建。
