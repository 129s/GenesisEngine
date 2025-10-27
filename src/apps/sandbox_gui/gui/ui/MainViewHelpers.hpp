#pragma once

#include "sandbox/gui/style/DesignTokens.hpp"
#include "sandbox/gui/style/LayoutMetrics.hpp"
#include "sandbox/gui/ui/LayoutHelpers.hpp"

#include <imgui.h>

#include <string>

namespace genesis::sandbox::gui
{

inline void PushActiveButtonStyle(bool active)
{
    if (active)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, Style::DesignTokens::color(Style::ColorToken::Primary));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Style::DesignTokens::color(Style::ColorToken::PrimaryHover));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Style::DesignTokens::color(Style::ColorToken::PrimaryActive));
    }
}

inline void PopActiveButtonStyle(bool active)
{
    if (active)
    {
        ImGui::PopStyleColor(3);
    }
}

inline void DrawOverlayToggleButton(const char* id, bool& value, const char* label, const char* tooltip)
{
    const bool initiallyActive = value;
    PushActiveButtonStyle(initiallyActive);
    std::string buttonLabel = std::string(label) + "##" + id;
    if (ImGui::Button(buttonLabel.c_str()))
    {
        value = !value;
    }
    Ui::applyClickableCursorToLastItem();
    if (tooltip && tooltip[0] != '\0' && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
    {
        ImGui::SetTooltip("%s", tooltip);
    }
    PopActiveButtonStyle(initiallyActive);
}

inline void CardSectionHeader(const Style::Layout::CardLayoutConfig& layout, const char* title, bool& firstSection)
{
    if (!firstSection)
    {
        ImGui::Dummy(ImVec2(0.0f, layout.sectionGap));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, layout.headerGap));
    }
    else
    {
        firstSection = false;
    }
    ImGui::TextUnformatted(title);
    ImGui::Dummy(ImVec2(0.0f, layout.lineGap));
}

} // namespace genesis::sandbox::gui

