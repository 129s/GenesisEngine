#include "sandbox/gui/ui/StatusBarView.hpp"
#include "sandbox/gui/AppHost.hpp"
#include "sandbox/gui/Version.hpp"
#include "sandbox/gui/style/DesignTokens.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <string_view>
#include <algorithm>
#include <cmath>

namespace Genesis::Sandbox::Gui
{

void StatusBarView::render(UiContext& ctx)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiStyle& style = ImGui::GetStyle();
    const float rawHeight = ImGui::GetFrameHeight() + style.FramePadding.y * 2.0f;
    const float axisSize = std::round(rawHeight);
    const float heightDiff = axisSize - rawHeight;
    const float verticalPadding = std::max(0.0f, style.FramePadding.y + heightDiff * 0.5f);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(Style::DesignTokens::spacing(Style::SpacingToken::Lg),
                               verticalPadding));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, Style::DesignTokens::windowBorderThickness());
    if (ImGui::BeginViewportSideBar("Status Bar", viewport, ImGuiDir_Down, axisSize, flags))
    {
        ImGui::AlignTextToFramePadding();
        std::string_view version = ctx.config.version.empty()
                                       ? SandboxGuiVersion
                                       : std::string_view(ctx.config.version);
        ImGui::Text("版本 %.*s", static_cast<int>(version.size()), version.data());

        ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
        const ImU32 borderColor =
            ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::BorderSoft));
        const float thickness = Style::DesignTokens::skeletonBorderThickness();
        const ImVec2 min = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        const float x0 = ImFloor(min.x);
        const float x1 = ImFloor(min.x + size.x);
        const float y0 = ImFloor(min.y);
        const float y1 = y0 + thickness;
        drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), borderColor);

    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace Genesis::Sandbox::Gui
