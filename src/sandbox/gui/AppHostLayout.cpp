#include "sandbox/gui/AppHost.hpp"

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <limits>

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
        ImGuiID dock_top = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Up, 0.08f, nullptr, &dock_main);
        ImGuiID dock_status = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.05f, nullptr, &dock_main);
        ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.28f, nullptr, &dock_main);
        ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.26f, nullptr, &dock_main);
        ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.22f, nullptr, &dock_main);
        ImGuiID dock_center = dock_main;

        ImGuiID dock_bottom_right = ImGui::DockBuilderSplitNode(dock_bottom, ImGuiDir_Right, 0.5f, nullptr, &dock_bottom);
        ImGuiID dock_bottom_left = dock_bottom;

        ImGui::DockBuilderDockWindow("Control Toolbar", dock_top);
        ImGui::DockBuilderDockWindow("Status Bar", dock_status);
        ImGui::DockBuilderDockWindow("Log Console", dock_bottom_left);
        ImGui::DockBuilderDockWindow("Telemetry", dock_bottom_right);
        ImGui::DockBuilderDockWindow("Scene View", dock_left);
        ImGui::DockBuilderDockWindow("World Generation", dock_left);
        ImGui::DockBuilderDockWindow("Inspector", dock_right);
        ImGui::DockBuilderDockWindow("Map View", dock_center);
        ImGui::DockBuilderDockWindow("Welcome", dock_center);

        ImGui::DockBuilderFinish(dockspace_id);
    }
}

void AppHost::drawMainMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Exit"))
            {
                glfwSetWindowShouldClose(window_, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Inspector", nullptr, &show_inspector_);
            ImGui::MenuItem("Map View", nullptr, &show_world_view_);
            ImGui::MenuItem("Scene View", nullptr, &show_scene_view_);
            ImGui::MenuItem("Telemetry", nullptr, &show_telemetry_);
            ImGui::MenuItem("World Generation", nullptr, &show_worldgen_panel_);
            ImGui::MenuItem("Log Console", nullptr, &show_logs_);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void AppHost::drawControlToolbar()
{
    const float menuHeight = ImGui::GetFrameHeight();
    const float paddingY = ImGui::GetStyle().FramePadding.y;
    const float height = menuHeight + paddingY * 2.0f;

    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, height), ImVec2(std::numeric_limits<float>::max(), height));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, paddingY));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                   ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("Control Toolbar", nullptr, flags))
    {
        if (runtime_bridge_)
        {
            bool paused = runtime_bridge_->paused();
            if (ImGui::Button(paused ? "Resume" : "Pause"))
            {
                runtime_bridge_->setPaused(!paused);
                pushToast(paused ? "Resume" : "Pause", ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
            }

            ImGui::SameLine();
            if (ImGui::Button("Step"))
            {
                runtime_bridge_->requestStep(1);
                pushToast("Step x1", ImVec4(0.8f, 0.86f, 0.98f, 1.0f));
            }

            ImGui::SameLine();
            if (ImGui::Button("Step x10"))
            {
                runtime_bridge_->requestStep(10);
                pushToast("Step x10", ImVec4(0.8f, 0.86f, 0.98f, 1.0f));
            }

            ImGui::SameLine(0.0f, 12.0f);
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine(0.0f, 12.0f);

            float speed = static_cast<float>(speed_multiplier_ui_);
            ImGui::SetNextItemWidth(180.0f);
            if (ImGui::SliderFloat("Speed", &speed, 0.25f, 8.0f, "%.2fx"))
            {
                speed_multiplier_ui_ = speed;
                runtime_bridge_->setSpeedMultiplier(speed_multiplier_ui_);
            }

            ImGui::SameLine(0.0f, 18.0f);
            if (ImGui::Checkbox("VSync", &config_.vsync))
            {
                glfwSwapInterval(config_.vsync ? 1 : 0);
            }

            if (latest_snapshot_)
            {
                ImGui::SameLine(0.0f, 18.0f);
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                ImGui::SameLine(0.0f, 12.0f);
                const auto& tick = latest_snapshot_->telemetry;
                ImGui::Text("Step %llu | Agents %zu | Resources %zu | Actions %zu",
                            static_cast<unsigned long long>(tick.step),
                            tick.agents.size(),
                            tick.resources.size(),
                            tick.actions.size());
            }
            else
            {
                ImGui::SameLine(0.0f, 18.0f);
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                ImGui::SameLine(0.0f, 12.0f);
                ImGui::TextUnformatted("Waiting for snapshot...");
            }
        }
        else
        {
            ImGui::TextUnformatted("RuntimeBridge unavailable.");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
}

void AppHost::drawStatusBar()
{
    const float height = ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.y;

    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, height), ImVec2(std::numeric_limits<float>::max(), height));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                   ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("Status Bar", nullptr, flags))
    {
        if (latest_snapshot_)
        {
            const auto& tick = latest_snapshot_->telemetry;
            const double totalSeconds = static_cast<double>(tick.step) * static_cast<double>(tick.stepSeconds);
            const std::uint64_t secs = static_cast<std::uint64_t>(totalSeconds);
            const std::uint64_t h = secs / 3600ULL;
            const std::uint64_t m = (secs % 3600ULL) / 60ULL;
            const std::uint64_t s = secs % 60ULL;
            ImGui::Text("Time %02llu:%02llu:%02llu | Step %llu | Agents %zu | Resources %zu | Actions %zu",
                        static_cast<unsigned long long>(h),
                        static_cast<unsigned long long>(m),
                        static_cast<unsigned long long>(s),
                        static_cast<unsigned long long>(tick.step),
                        tick.agents.size(),
                        tick.resources.size(),
                        tick.actions.size());
        }
        else
        {
            ImGui::TextUnformatted("Simulation warming up…");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
}

} // namespace Genesis::Sandbox::Gui
