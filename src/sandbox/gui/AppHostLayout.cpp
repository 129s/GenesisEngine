#include "sandbox/gui/AppHost.hpp"
#include "sandbox/gui/style/DesignTokens.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace Genesis::Sandbox::Gui
{

void AppHost::drawDockspace()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    bool layout_locked = (!ui_state_.layout_mode_enabled) && dock_layout_initialized_;
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_AutoHideTabBar;
    if (layout_locked)
    {
        dockspace_flags |= ImGuiDockNodeFlags_NoSplit | ImGuiDockNodeFlags_NoUndocking | ImGuiDockNodeFlags_NoResize;
    }
    ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, viewport, dockspace_flags);

    ImGuiDockNode* rootNode = ImGui::DockBuilderGetNode(dockspace_id);
    if (!dock_layout_initialized_ && (rootNode == nullptr || (!rootNode->IsSplitNode() && rootNode->Windows.Size == 0)))
    {
        dock_layout_initialized_ = true;
        layout_locked = !ui_state_.layout_mode_enabled;

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

        ImGuiID dock_main = dockspace_id;
        ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.23f, nullptr, &dock_main);

        ImGui::DockBuilderDockWindow("Browser", dock_left);
        ImGui::DockBuilderDockWindow("Palette", dock_left);
        ImGui::DockBuilderDockWindow("Main View", dock_main);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    if (rootNode != nullptr)
    {
        constexpr ImGuiDockNodeFlags lockFlags =
            ImGuiDockNodeFlags_NoSplit | ImGuiDockNodeFlags_NoUndocking | ImGuiDockNodeFlags_NoResize;

        auto applyFlags = [&](ImGuiDockNode* node, const auto& self) -> void {
            if (node == nullptr)
            {
                return;
            }

            node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
            if (layout_locked)
            {
                node->LocalFlags |= lockFlags;
            }
            else
            {
                node->LocalFlags &= ~lockFlags;
            }

            if (node->ChildNodes[0] != nullptr)
            {
                self(node->ChildNodes[0], self);
            }
            if (node->ChildNodes[1] != nullptr)
            {
                self(node->ChildNodes[1], self);
            }
        };

        applyFlags(rootNode, applyFlags);
    }
}

void AppHost::drawOuterFrame()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr)
    {
        return;
    }

    const float thickness = Style::DesignTokens::skeletonBorderThickness();
    const ImU32 borderColor =
        ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::BorderSoft));
    const float left = ImFloor(viewport->Pos.x);
    const float top = ImFloor(viewport->Pos.y);
    const float right = ImFloor(viewport->Pos.x + viewport->Size.x);
    const float bottom = ImFloor(viewport->Pos.y + viewport->Size.y);

    ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
    // top
    drawList->AddRectFilled(ImVec2(left, top), ImVec2(right, top + thickness), borderColor);
    // bottom
    drawList->AddRectFilled(ImVec2(left, bottom - thickness), ImVec2(right, bottom), borderColor);
    // left
    drawList->AddRectFilled(ImVec2(left, top), ImVec2(left + thickness, bottom), borderColor);
    // right
    drawList->AddRectFilled(ImVec2(right - thickness, top), ImVec2(right, bottom), borderColor);
}

} // namespace Genesis::Sandbox::Gui
