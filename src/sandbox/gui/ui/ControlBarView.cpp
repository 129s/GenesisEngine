#include "sandbox/gui/ui/ControlBarView.hpp"
#include "sandbox/gui/AppHost.hpp"
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

void ControlBarView::render(UiContext& ctx)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiStyle& style = ImGui::GetStyle();
    const float height = ImGui::GetFrameHeight() + style.FramePadding.y * 2.2f;

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowPadding,
        ImVec2(Style::DesignTokens::spacing(Style::SpacingToken::Lg),
               style.FramePadding.y));
    const bool open = ImGui::BeginViewportSideBar("Control Bar", viewport, ImGuiDir_Up, height, flags);
    if (open)
    {
        const float navSpacing = Style::DesignTokens::spacing(Style::SpacingToken::Md);
        auto drawNavButton = [&](const char* label, MainViewTab tab, bool first) {
            if (!first)
            {
                ImGui::SameLine(0.0f, navSpacing);
            }
            const bool active = (ctx.state.main_view_active_tab == tab);
            PushActiveButtonStyle(active);
            if (ImGui::Button(label))
            {
                ctx.state.main_view_active_tab = tab;
            }
            PopActiveButtonStyle(active);
        };

        drawNavButton("Monitor", MainViewTab::Monitor, true);
        drawNavButton("Scene", MainViewTab::Scene, false);
        drawNavButton("World", MainViewTab::World, false);
        drawNavButton("Settings", MainViewTab::Settings, false);

        ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Lg));
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Lg));

        if (ctx.runtime_bridge)
        {
            bool paused = ctx.runtime_bridge->paused();
            if (ImGui::Button(paused ? "继续" : "暂停"))
            {
                ctx.runtime_bridge->setPaused(!paused);
                ctx.pushToast(paused ? "Resume" : "Pause", Style::DesignTokens::color(Style::ColorToken::Info));
            }

            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Sm));
            if (ImGui::Button("单步"))
            {
                ctx.runtime_bridge->requestStep(1);
                ctx.pushToast("Step x1", Style::DesignTokens::color(Style::ColorToken::Accent));
            }

            ImGui::SameLine();
            if (ImGui::Button("快进×10"))
            {
                ctx.runtime_bridge->requestStep(10);
                ctx.pushToast("Step x10", Style::DesignTokens::color(Style::ColorToken::AccentHover));
            }

            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Lg));
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Lg));

            float speed = static_cast<float>(ctx.speed_multiplier_ui);
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::SliderFloat("速度倍率", &speed, 0.25f, 8.0f, "%.2fx"))
            {
                ctx.speed_multiplier_ui = speed;
                ctx.runtime_bridge->setSpeedMultiplier(ctx.speed_multiplier_ui);
            }

            if (ctx.latest_snapshot)
            {
                ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Lg));
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Lg));
                ImGui::TextUnformatted("快照同步就绪");
            }
            else
            {
                ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Lg));
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Lg));
                ImGui::TextUnformatted("等待快照同步…");
            }
        }
        else
        {
            ImGui::TextUnformatted("RuntimeBridge unavailable.");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace Genesis::Sandbox::Gui
