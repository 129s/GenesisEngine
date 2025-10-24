#include "sandbox/gui/style/DesignTokens.hpp"

#include <optional>

#include <spdlog/spdlog.h>

#include <array>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string_view>

namespace Genesis::Sandbox::Gui::Style
{
namespace
{
    using ColorArray = std::array<ImVec4, static_cast<std::size_t>(ColorToken::Count)>;
    using SpacingArray = std::array<float, static_cast<std::size_t>(SpacingToken::Count)>;

    const ColorArray kFallbackColors = {
        /* Canvas */ ImVec4(0.098f, 0.114f, 0.129f, 1.0f),        // #191D21
        /* Surface */ ImVec4(0.125f, 0.149f, 0.169f, 1.0f),       // #20262B
        /* SurfaceAlt */ ImVec4(0.137f, 0.161f, 0.180f, 1.0f),    // #23292E
        /* SurfaceActive */ ImVec4(0.153f, 0.173f, 0.196f, 1.0f), // #272C32
        /* Primary */ ImVec4(0.196f, 0.765f, 1.000f, 1.0f),       // #32C3FF
        /* PrimaryHover */ ImVec4(0.318f, 0.800f, 1.000f, 1.0f),  // #51CCFF
        /* PrimaryActive */ ImVec4(0.000f, 0.706f, 1.000f, 1.0f), // #00B4FF
        /* TextPrimary */ ImVec4(0.694f, 0.773f, 0.800f, 1.0f),   // #B1C5CC
        /* TextSecondary */ ImVec4(0.506f, 0.576f, 0.631f, 1.0f), // #8193A1
        /* TextDisabled */ ImVec4(0.384f, 0.439f, 0.475f, 1.0f),  // #627079
        /* BorderSoft */ ImVec4(0.184f, 0.224f, 0.239f, 1.0f),    // #2F393D
        /* BorderStrong */ ImVec4(0.031f, 0.035f, 0.039f, 1.0f),  // #08090A
        /* Accent */ ImVec4(0.251f, 0.710f, 0.627f, 1.0f),        // #40B5A0
        /* AccentHover */ ImVec4(0.302f, 0.753f, 0.675f, 1.0f),   // #4DC0AC
        /* AccentActive */ ImVec4(0.212f, 0.604f, 0.533f, 1.0f),  // #369A88
        /* Success */ ImVec4(0.424f, 1.000f, 0.000f, 1.0f),       // #6CFF00
        /* Warning */ ImVec4(1.000f, 0.784f, 0.341f, 1.0f),       // #FFC857
        /* Danger */ ImVec4(1.000f, 0.420f, 0.420f, 1.0f),        // #FF6B6B
        /* Info */ ImVec4(0.196f, 0.765f, 1.000f, 1.0f),          // #32C3FF
        /* Muted */ ImVec4(0.161f, 0.196f, 0.204f, 1.0f),         // #293234
        /* Highlight */ ImVec4(0.855f, 0.937f, 1.000f, 1.0f),     // #DAEFFF
    };

    constexpr SpacingArray kSpacing = {
        0.0f,  // None
        4.0f,  // Xs
        8.0f,  // Sm
        12.0f, // Md
        16.0f, // Lg
        24.0f, // Xl
    };

    using TokenNameArray = std::array<std::string_view, static_cast<std::size_t>(ColorToken::Count)>;

    const TokenNameArray kPaletteTokenNames = {
        "surface.canvas",
        "surface.layer",
        "surface.layer.alt",
        "surface.layer.active",
        "accent.primary.base",
        "accent.primary.hover",
        "accent.primary.active",
        "text.primary",
        "text.secondary",
        "text.disabled",
        "border.soft",
        "border.strong",
        "accent.secondary.base",
        "accent.secondary.hover",
        "accent.secondary.active",
        "status.success",
        "status.warning",
        "status.danger",
        "status.info",
        "surface.muted",
        "state.highlight",
    };

    // removed ToImVec (palette disabled)

    void EnsurePaletteLoaded() {}

    std::optional<ImVec4> ResolvePaletteColor(ColorToken token)
    {
        return std::nullopt;
    }
} // namespace

void DesignTokens::applyTo(ImGuiStyle &style)
{
    EnsurePaletteLoaded();

    style.WindowPadding = ImVec2(0.0f, 0.0f);
    style.FramePadding = ImVec2(0.0f, 0.0f);
    style.CellPadding = ImVec2(0.0f, 0.0f);
    style.ItemSpacing = ImVec2(0.0f, 0.0f);
    style.ItemInnerSpacing = ImVec2(0.0f, 0.0f);
    style.IndentSpacing = 0.0f;
    style.ScrollbarSize = scrollbarSize();

    style.WindowRounding = roundingNone();
    style.ChildRounding = roundingNone();
    style.FrameRounding = roundingNone();
    style.PopupRounding = roundingNone();
    style.ScrollbarRounding = roundingNone();
    style.GrabRounding = roundingNone();
    style.TabRounding = roundingNone();

    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;
    style.SeparatorTextBorderSize = 0.0f;
    style.DockingSeparatorSize = 2.0f;
    style.TouchExtraPadding = ImVec2(6.0f, 6.0f);

    style.Colors[ImGuiCol_Text] = color(ColorToken::TextPrimary);
    style.Colors[ImGuiCol_TextDisabled] = color(ColorToken::TextDisabled);
    style.Colors[ImGuiCol_WindowBg] = color(ColorToken::Canvas);
    style.Colors[ImGuiCol_ChildBg] = color(ColorToken::Surface);
    style.Colors[ImGuiCol_PopupBg] = color(ColorToken::SurfaceAlt);
    style.Colors[ImGuiCol_Border] = color(ColorToken::BorderSoft);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

    style.Colors[ImGuiCol_FrameBg] = color(ColorToken::Surface);
    style.Colors[ImGuiCol_FrameBgHovered] = color(ColorToken::SurfaceActive);
    style.Colors[ImGuiCol_FrameBgActive] = color(ColorToken::PrimaryActive);

    style.Colors[ImGuiCol_TitleBg] = color(ColorToken::Surface);
    style.Colors[ImGuiCol_TitleBgActive] = color(ColorToken::SurfaceAlt);
    style.Colors[ImGuiCol_TitleBgCollapsed] = color(ColorToken::Surface);
    style.Colors[ImGuiCol_MenuBarBg] = color(ColorToken::SurfaceAlt);

    style.Colors[ImGuiCol_ScrollbarBg] = color(ColorToken::Surface);
    style.Colors[ImGuiCol_ScrollbarGrab] = color(ColorToken::Muted);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = color(ColorToken::SurfaceActive);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = color(ColorToken::SurfaceActive);

    style.Colors[ImGuiCol_CheckMark] = color(ColorToken::Primary);
    style.Colors[ImGuiCol_SliderGrab] = color(ColorToken::Accent);
    style.Colors[ImGuiCol_SliderGrabActive] = color(ColorToken::AccentActive);
    style.Colors[ImGuiCol_Button] = color(ColorToken::Primary);
    style.Colors[ImGuiCol_ButtonHovered] = color(ColorToken::PrimaryHover);
    style.Colors[ImGuiCol_ButtonActive] = color(ColorToken::PrimaryActive);

    style.Colors[ImGuiCol_Header] = color(ColorToken::SurfaceAlt);
    style.Colors[ImGuiCol_HeaderHovered] = color(ColorToken::SurfaceActive);
    style.Colors[ImGuiCol_HeaderActive] = color(ColorToken::PrimaryActive);

    style.Colors[ImGuiCol_Separator] = color(ColorToken::BorderSoft);
    style.Colors[ImGuiCol_SeparatorHovered] = color(ColorToken::AccentHover);
    style.Colors[ImGuiCol_SeparatorActive] = color(ColorToken::AccentActive);

    style.Colors[ImGuiCol_ResizeGrip] = color(ColorToken::Muted);
    style.Colors[ImGuiCol_ResizeGripHovered] = color(ColorToken::SurfaceActive);
    style.Colors[ImGuiCol_ResizeGripActive] = color(ColorToken::AccentActive);

    style.Colors[ImGuiCol_Tab] = color(ColorToken::SurfaceAlt);
    style.Colors[ImGuiCol_TabHovered] = color(ColorToken::PrimaryHover);
    style.Colors[ImGuiCol_TabActive] = color(ColorToken::PrimaryActive);
    style.Colors[ImGuiCol_TabUnfocused] = color(ColorToken::SurfaceAlt);
    style.Colors[ImGuiCol_TabUnfocusedActive] = color(ColorToken::SurfaceActive);

    style.Colors[ImGuiCol_PlotLines] = color(ColorToken::Accent);
    style.Colors[ImGuiCol_PlotLinesHovered] = color(ColorToken::AccentHover);
    style.Colors[ImGuiCol_PlotHistogram] = color(ColorToken::Accent);
    style.Colors[ImGuiCol_PlotHistogramHovered] = color(ColorToken::AccentHover);

    style.Colors[ImGuiCol_TableHeaderBg] = color(ColorToken::SurfaceAlt);
    style.Colors[ImGuiCol_TableBorderStrong] = color(ColorToken::BorderStrong);
    style.Colors[ImGuiCol_TableBorderLight] = color(ColorToken::BorderSoft);
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(color(ColorToken::SurfaceAlt).x,
                                                  color(ColorToken::SurfaceAlt).y,
                                                  color(ColorToken::SurfaceAlt).z,
                                                  0.35f);

    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(color(ColorToken::Primary).x,
                                                   color(ColorToken::Primary).y,
                                                   color(ColorToken::Primary).z,
                                                   0.35f);
    style.Colors[ImGuiCol_DragDropTarget] = color(ColorToken::Highlight);
    style.Colors[ImGuiCol_DockingPreview] = color(ColorToken::Accent);
    style.Colors[ImGuiCol_NavHighlight] = color(ColorToken::PrimaryHover);
    style.Colors[ImGuiCol_NavWindowingHighlight] = color(ColorToken::PrimaryHover);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, 0.45f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.55f);
}

ImVec4 DesignTokens::color(ColorToken token)
{
    if (const auto fromPalette = ResolvePaletteColor(token))
    {
        return *fromPalette;
    }
    return kFallbackColors[static_cast<std::size_t>(token)];
}

float DesignTokens::spacing(SpacingToken token)
{
    return kSpacing[static_cast<std::size_t>(token)];
}

} // namespace Genesis::Sandbox::Gui::Style

