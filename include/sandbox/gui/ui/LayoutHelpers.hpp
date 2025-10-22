#pragma once

#include <imgui.h>

namespace Genesis::Sandbox::Gui::Ui
{

// 在布局模式下为当前窗口绘制可拖拽的停靠锚点。
// 传入唯一的 id 后缀用于生成 ImGui ID。
void drawDockAnchorOverlay(const char* id_suffix, bool layout_mode_enabled);

} // namespace Genesis::Sandbox::Gui::Ui

