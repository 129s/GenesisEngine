#pragma once

#include "sandbox/gui/style/DesignTokens.hpp"

#include <imgui.h>

namespace Genesis::Sandbox::Gui::Style::Layout
{

struct CardLayoutConfig
{
    ImVec2 padding{
        DesignTokens::spacing(SpacingToken::Md),
        DesignTokens::spacing(SpacingToken::Sm)};
    float headerGap{DesignTokens::spacing(SpacingToken::Xs)};
    float sectionGap{DesignTokens::spacing(SpacingToken::Sm)};
    float lineGap{DesignTokens::spacing(SpacingToken::Xs)};
    float indent{DesignTokens::spacing(SpacingToken::Sm)};
    ImVec4 background{DesignTokens::color(ColorToken::Surface)};
};

CardLayoutConfig detailCard();

class CardScope
{
public:
    CardScope(const char* id,
              const CardLayoutConfig& config,
              ImGuiWindowFlags extraFlags = 0,
              ImVec2 size = ImVec2(0.0f, 0.0f));
    ~CardScope();

    bool isOpen() const { return open_; }

private:
    bool open_{false};
    int styleCount_{0};
    bool colorPushed_{false};
};

struct WindowLayoutConfig
{
    ImVec2 windowPadding{
        DesignTokens::spacing(SpacingToken::Lg),
        DesignTokens::spacing(SpacingToken::Lg)};
    ImVec2 itemSpacing{
        DesignTokens::spacing(SpacingToken::Sm),
        DesignTokens::spacing(SpacingToken::Sm)};
    ImVec2 framePadding{
        DesignTokens::spacing(SpacingToken::Sm),
        DesignTokens::spacing(SpacingToken::Xs)};
};

WindowLayoutConfig mainViewWindow();

class WindowStyleScope
{
public:
    explicit WindowStyleScope(const WindowLayoutConfig& config);
    ~WindowStyleScope();

private:
    int styleCount_{0};
};

struct BarLayoutConfig
{
    float paddingX{DesignTokens::spacing(SpacingToken::Lg)};
    float paddingY{DesignTokens::spacing(SpacingToken::Xs)};
    float framePaddingX{DesignTokens::spacing(SpacingToken::Sm)};
    float framePaddingY{DesignTokens::spacing(SpacingToken::Xs)};
    float itemSpacingX{DesignTokens::spacing(SpacingToken::Sm)};
    float itemSpacingY{0.0f};
    float windowBorderSize{0.0f};
};

constexpr ImGuiWindowFlags kDefaultBarWindowFlags =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

BarLayoutConfig statusBar();
BarLayoutConfig controlBar();

class BarScope
{
public:
    BarScope(const char* label,
             ImGuiViewport* viewport,
             ImGuiDir direction,
             const BarLayoutConfig& config,
             ImGuiWindowFlags flags);
    ~BarScope();

    bool isOpen() const { return open_; }
    float axisSize() const { return axisSize_; }

private:
    bool open_{false};
    float axisSize_{0.0f};
    int styleCount_{0};
};

} // namespace Genesis::Sandbox::Gui::Style::Layout
