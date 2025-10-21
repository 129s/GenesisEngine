#include "sandbox/gui/AppHost.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace Genesis::Sandbox::Gui
{

void AppHost::drawDockspace()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

    ImGuiDockNode* rootNode = ImGui::DockBuilderGetNode(dockspace_id);
    if (!dock_layout_initialized_ && (rootNode == nullptr || (!rootNode->IsSplitNode() && rootNode->Windows.Size == 0)))
    {
        dock_layout_initialized_ = true;

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

        ImGuiID dock_main = dockspace_id;
        ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.23f, nullptr, &dock_main);
        ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.28f, nullptr, &dock_main);

        ImGui::DockBuilderDockWindow("Browser", dock_left);
        ImGui::DockBuilderDockWindow("Main View", dock_main);
        ImGui::DockBuilderDockWindow("Inspector", dock_right);

        ImGui::DockBuilderFinish(dockspace_id);
    }
}

} // namespace Genesis::Sandbox::Gui
