#include "sandbox/gui/AppHost.hpp"

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_internal.h>

namespace Genesis::Sandbox::Gui
{

void AppHost::drawDockspace()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::DockSpaceOverViewport(0, viewport, ImGuiDockNodeFlags_PassthruCentralNode);
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
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float menuHeight = ImGui::GetFrameHeight();
    const float paddingY = ImGui::GetStyle().FramePadding.y;
    const float height = menuHeight + paddingY * 2.0f;

    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + menuHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, height));
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, paddingY));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

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
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float height = ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.y;

    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - height));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, height));
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs |
                                   ImGuiWindowFlags_NoDocking;
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

