#include "sandbox/gui/ui/LayoutHelpers.hpp"

#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>

namespace Genesis::Sandbox::Gui::Ui
{

void drawDockAnchorOverlay(const char* id_suffix, bool layout_mode_enabled)
{
    if (!layout_mode_enabled)
    {
        return;
    }

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window == nullptr || window->DockNode == nullptr)
    {
        return;
    }

    const ImVec2 original_cursor = ImGui::GetCursorScreenPos();
    const float size = std::max(18.0f, ImGui::GetFontSize() * 1.2f);
    const float padding = 6.0f;
    const ImVec2 anchor_pos(window->Pos.x + padding, window->Pos.y + padding);

    ImGui::SetCursorScreenPos(anchor_pos);
    ImGui::PushID(id_suffix);
    ImGui::InvisibleButton("dock_anchor", ImVec2(size, size));

    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup |
                                              ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const bool active = ImGui::IsItemActive();

    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
    {
        ImGui::StartMouseMovingWindowOrNode(window, window->DockNode, true);
    }

    ImDrawList* draw_list = ImGui::GetForegroundDrawList(window->Viewport);
    const ImU32 fill_color = hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
                                     : ImGui::GetColorU32(ImGuiCol_Button);
    const ImU32 border_color = ImGui::GetColorU32(ImGuiCol_Border);
    const ImU32 glyph_color = ImGui::GetColorU32(ImGuiCol_Text);

    const ImVec2 rect_min = anchor_pos;
    const ImVec2 rect_max(anchor_pos.x + size, anchor_pos.y + size);
    const float rounding = 4.0f;

    draw_list->AddRectFilled(rect_min, rect_max, fill_color, rounding);
    draw_list->AddRect(rect_min, rect_max, border_color, rounding);

    const ImVec2 center = ImVec2((rect_min.x + rect_max.x) * 0.5f, (rect_min.y + rect_max.y) * 0.5f);
    const float cross = size * 0.35f;
    const ImVec2 horiz_start(center.x - cross, center.y);
    const ImVec2 horiz_end(center.x + cross, center.y);
    const ImVec2 vert_start(center.x, center.y - cross);
    const ImVec2 vert_end(center.x, center.y + cross);
    draw_list->AddLine(horiz_start, horiz_end, glyph_color, 1.8f);
    draw_list->AddLine(vert_start, vert_end, glyph_color, 1.8f);

    ImGui::PopID();
    ImGui::SetCursorScreenPos(original_cursor);
}

} // namespace Genesis::Sandbox::Gui::Ui
