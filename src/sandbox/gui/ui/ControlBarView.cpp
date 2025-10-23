#include "sandbox/gui/ui/ControlBarView.hpp"
#include "sandbox/gui/AppHost.hpp"
#include "sandbox/gui/style/DesignTokens.hpp"
#include "sandbox/gui/style/LayoutMetrics.hpp"
#include "sandbox/gui/ui/LayoutHelpers.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>

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
    const auto layout = Style::Layout::controlBar();
    Style::Layout::BarScope bar("Control Bar",
                                viewport,
                                ImGuiDir_Up,
                                layout,
                                Style::Layout::kDefaultBarWindowFlags);
    if (bar.isOpen())
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
            Ui::applyClickableCursorToLastItem();
            PopActiveButtonStyle(active);
        };

        drawNavButton("Monitor", MainViewTab::Monitor, true);
        drawNavButton("Scene", MainViewTab::Scene, false);
        drawNavButton("World", MainViewTab::World, false);
        drawNavButton("Settings", MainViewTab::Settings, false);

        ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Lg));
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Lg));

        bool layoutMode = ctx.state.layout_mode_enabled;
        PushActiveButtonStyle(layoutMode);
        if (ImGui::Button("Layout"))
        {
            ctx.state.layout_mode_enabled = !ctx.state.layout_mode_enabled;
            layoutMode = ctx.state.layout_mode_enabled;
            ctx.pushToast(layoutMode ? "布局模式：开启" : "布局模式：关闭",
                          Style::DesignTokens::color(layoutMode ? Style::ColorToken::Accent
                                                                 : Style::ColorToken::Muted));
        }
        Ui::applyClickableCursorToLastItem();
        PopActiveButtonStyle(layoutMode);

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
            Ui::applyClickableCursorToLastItem();

            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Sm));
            if (ImGui::Button("单步"))
            {
                ctx.runtime_bridge->requestStep(1);
                ctx.pushToast("Step x1", Style::DesignTokens::color(Style::ColorToken::Accent));
            }
            Ui::applyClickableCursorToLastItem();

            ImGui::SameLine();
            if (ImGui::Button("快进×10"))
            {
                ctx.runtime_bridge->requestStep(10);
                ctx.pushToast("Step x10", Style::DesignTokens::color(Style::ColorToken::AccentHover));
            }
            Ui::applyClickableCursorToLastItem();

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

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 borderColor =
            ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::BorderSoft));
        const float thickness = Style::DesignTokens::skeletonBorderThickness();
        const ImVec2 min = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        const float x0 = ImFloor(min.x);
        const float x1 = ImFloor(min.x + size.x);
        const float y1 = ImFloor(min.y + size.y);
        const float y0 = y1 - thickness;
        drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), borderColor);
    }
}

} // namespace Genesis::Sandbox::Gui
