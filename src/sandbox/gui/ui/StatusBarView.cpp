#include "sandbox/gui/ui/StatusBarView.hpp"
#include "sandbox/gui/AppHost.hpp"
#include "sandbox/gui/Version.hpp"
#include "sandbox/gui/style/DesignTokens.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <string_view>

namespace Genesis::Sandbox::Gui
{

void StatusBarView::render(UiContext& ctx)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiStyle& style = ImGui::GetStyle();
    const float height = ImGui::GetFrameHeight() + style.FramePadding.y * 2.0f;

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(Style::DesignTokens::spacing(Style::SpacingToken::Lg),
                               style.FramePadding.y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, Style::DesignTokens::skeletonBorderThickness());
    if (ImGui::BeginViewportSideBar("Status Bar", viewport, ImGuiDir_Down, height, flags))
    {
        ImGui::AlignTextToFramePadding();
        std::string_view version = ctx.config.version.empty()
                                       ? SandboxGuiVersion
                                       : std::string_view(ctx.config.version);
        ImGui::Text("版本 %.*s", static_cast<int>(version.size()), version.data());

        const ImU32 borderColor =
            ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::BorderSoft));
        const float thickness = Style::DesignTokens::skeletonBorderThickness();
        const ImVec2 min = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        const ImVec2 max(min.x + size.x, min.y + size.y);
        ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x, min.y), ImVec2(max.x, min.y), borderColor, thickness);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace Genesis::Sandbox::Gui
