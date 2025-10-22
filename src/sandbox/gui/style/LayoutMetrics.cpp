#include "sandbox/gui/style/LayoutMetrics.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>

namespace Genesis::Sandbox::Gui::Style::Layout
{

CardLayoutConfig detailCard()
{
    return CardLayoutConfig{};
}

CardScope::CardScope(const char* id,
                     const CardLayoutConfig& config,
                     ImGuiWindowFlags extraFlags,
                     ImVec2 size)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, config.padding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0.0f, 0.0f));
    styleCount_ = 3;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, config.background);
    colorPushed_ = true;
    open_ = ImGui::BeginChild(id, size, false, extraFlags | ImGuiWindowFlags_AlwaysUseWindowPadding);
    if (!open_)
    {
        ImGui::PopStyleColor();
        colorPushed_ = false;
    }
}

CardScope::~CardScope()
{
    if (open_)
    {
        ImGui::EndChild();
    }
    if (colorPushed_)
    {
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar(styleCount_);
}

WindowLayoutConfig mainViewWindow()
{
    return WindowLayoutConfig{};
}

WindowStyleScope::WindowStyleScope(const WindowLayoutConfig& config)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, config.windowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, config.itemSpacing);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, config.framePadding);
    styleCount_ = 3;
}

WindowStyleScope::~WindowStyleScope()
{
    ImGui::PopStyleVar(styleCount_);
}

BarLayoutConfig statusBar()
{
    BarLayoutConfig cfg{};
    cfg.paddingY = DesignTokens::spacing(SpacingToken::Xs);
    cfg.framePaddingY = DesignTokens::spacing(SpacingToken::Xs);
    cfg.itemSpacingX = DesignTokens::spacing(SpacingToken::Sm);
    cfg.itemSpacingY = 0.0f;
    return cfg;
}

BarLayoutConfig controlBar()
{
    BarLayoutConfig cfg{};
    cfg.paddingY = DesignTokens::spacing(SpacingToken::Xs);
    cfg.framePaddingY = DesignTokens::spacing(SpacingToken::Xs);
    cfg.itemSpacingX = DesignTokens::spacing(SpacingToken::Sm);
    cfg.itemSpacingY = 0.0f;
    return cfg;
}

BarScope::BarScope(const char* label,
                   ImGuiViewport* viewport,
                   ImGuiDir direction,
                   const BarLayoutConfig& config,
                   ImGuiWindowFlags flags)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(config.framePaddingX, config.framePaddingY));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(config.paddingX, config.paddingY));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(config.itemSpacingX, config.itemSpacingY));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, config.windowBorderSize);
    styleCount_ = 4;

    const float baseHeight = ImGui::GetFontSize() + config.framePaddingY * 2.0f;
    axisSize_ = std::round(baseHeight + config.paddingY * 2.0f);

    open_ = ImGui::BeginViewportSideBar(label, viewport, direction, axisSize_, flags);
}

BarScope::~BarScope()
{
    ImGui::End();
    ImGui::PopStyleVar(styleCount_);
}

} // namespace Genesis::Sandbox::Gui::Style::Layout
