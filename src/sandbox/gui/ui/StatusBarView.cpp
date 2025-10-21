#include "sandbox/gui/ui/StatusBarView.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace Genesis::Sandbox::Gui
{
namespace
{
    void PushActiveButtonStyle(bool active)
    {
        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.80f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.50f, 0.88f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.40f, 0.72f, 1.0f));
        }
    }

    void PopActiveButtonStyle(bool active)
    {
        if (active)
        {
            ImGui::PopStyleColor(3);
        }
    }
} // namespace

void StatusBarView::render(UiContext& ctx)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiStyle& style = ImGui::GetStyle();
    const float height = ImGui::GetFrameHeight() + style.FramePadding.y + 4.0f;

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 4.0f));
    if (ImGui::BeginViewportSideBar("Status Bar", viewport, ImGuiDir_Up, height, flags))
    {
        ImGui::AlignTextToFramePadding();

        if (ctx.runtime_bridge)
        {
            const bool paused = ctx.runtime_bridge->paused();
            ImGui::TextUnformatted(paused ? "状态：Paused" : "状态：Running");
        }
        else
        {
            ImGui::TextUnformatted("状态：Runtime offline");
        }

        ImGui::SameLine(0.0f, 18.0f);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0.0f, 18.0f);

        ImGui::Text("Speed %.2fx", static_cast<float>(ctx.speed_multiplier_ui));

        auto drawNavButton = [&](const char* label, MainViewTab tab, BrowserSection section) {
            ImGui::SameLine(0.0f, 12.0f);
            const bool active = (ctx.state.main_view_active_tab == tab);
            PushActiveButtonStyle(active);
            if (ImGui::Button(label))
            {
                ctx.state.main_view_active_tab = tab;
                ctx.state.browser_active_section = section;
            }
            PopActiveButtonStyle(active);
        };

        drawNavButton("Monitor", MainViewTab::Monitor, BrowserSection::Monitor);
        drawNavButton("Scene", MainViewTab::Scene, BrowserSection::Scene);
        drawNavButton("World", MainViewTab::World, BrowserSection::World);
        drawNavButton("Settings", MainViewTab::Settings, BrowserSection::LayoutsThemes);

        ImGui::SameLine(0.0f, 18.0f);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0.0f, 18.0f);

        if (ctx.latest_snapshot)
        {
            const auto& tick = ctx.latest_snapshot->telemetry;
            ImGui::Text("Step %llu", static_cast<unsigned long long>(tick.step));
            ImGui::SameLine(0.0f, 12.0f);
            ImGui::Text("Agents %zu", tick.agents.size());
            ImGui::SameLine(0.0f, 12.0f);
            ImGui::Text("Resources %zu", tick.resources.size());
            ImGui::SameLine(0.0f, 12.0f);
            const std::size_t alertCount = ctx.latest_snapshot->events.size();
            if (alertCount > 0)
            {
                ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.45f, 1.0f), "Alerts %zu", alertCount);
            }
            else
            {
                ImGui::TextDisabled("Alerts 0");
            }
        }
        else
        {
            ImGui::TextUnformatted("等待首帧快照…");
        }

        ImGui::SameLine(0.0f, 18.0f);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0.0f, 12.0f);
        ImGui::TextUnformatted("Help ▸ F1");
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace Genesis::Sandbox::Gui
