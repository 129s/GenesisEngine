#include "sandbox/gui/ui/StatusBarView.hpp"
#include "sandbox/gui/style/DesignTokens.hpp"

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
            ImGui::PushStyleColor(ImGuiCol_Button, Style::DesignTokens::color(Style::ColorToken::Primary));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Style::DesignTokens::color(Style::ColorToken::PrimaryHover));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Style::DesignTokens::color(Style::ColorToken::PrimaryActive));
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
    const float height = ImGui::GetFrameHeight() + style.FramePadding.y + Style::DesignTokens::spacing(Style::SpacingToken::Xs);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(Style::DesignTokens::spacing(Style::SpacingToken::Lg),
                               Style::DesignTokens::spacing(Style::SpacingToken::Xs)));
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

        ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Lg));
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Lg));

        ImGui::Text("Speed %.2fx", static_cast<float>(ctx.speed_multiplier_ui));

        auto drawNavButton = [&](const char* label, MainViewTab tab, BrowserSection section) {
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Md));
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

        ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Lg));
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Lg));

        if (ctx.latest_snapshot)
        {
            const auto& tick = ctx.latest_snapshot->telemetry;
            ImGui::Text("Step %llu", static_cast<unsigned long long>(tick.step));
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Md));
            ImGui::Text("Agents %zu", tick.agents.size());
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Md));
            ImGui::Text("Resources %zu", tick.resources.size());
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Md));
            const std::size_t alertCount = ctx.latest_snapshot->events.size();
            if (alertCount > 0)
            {
                ImGui::TextColored(Style::DesignTokens::color(Style::ColorToken::Danger), "Alerts %zu", alertCount);
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

        ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Lg));
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Md));
        ImGui::TextUnformatted("Help ▸ F1");
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace Genesis::Sandbox::Gui
