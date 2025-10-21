#include "sandbox/gui/style/DesignTokens.hpp"

#include <array>

namespace Genesis::Sandbox::Gui::Style
{
namespace
{
    using ColorArray = std::array<ImVec4, static_cast<std::size_t>(ColorToken::Count)>;
    using SpacingArray = std::array<float, static_cast<std::size_t>(SpacingToken::Count)>;

    const ColorArray kColors = {
        /* Canvas */ ImVec4(0.07f, 0.08f, 0.10f, 1.0f),
        /* Surface */ ImVec4(0.11f, 0.12f, 0.15f, 1.0f),
        /* SurfaceAlt */ ImVec4(0.14f, 0.16f, 0.19f, 1.0f),
        /* SurfaceActive */ ImVec4(0.18f, 0.20f, 0.24f, 1.0f),
        /* Primary */ ImVec4(0.18f, 0.42f, 0.82f, 0.95f),
        /* PrimaryHover */ ImVec4(0.22f, 0.48f, 0.90f, 1.0f),
        /* PrimaryActive */ ImVec4(0.17f, 0.37f, 0.70f, 1.0f),
        /* TextPrimary */ ImVec4(0.90f, 0.93f, 0.98f, 1.0f),
        /* TextSecondary */ ImVec4(0.72f, 0.78f, 0.86f, 1.0f),
        /* TextDisabled */ ImVec4(0.47f, 0.52f, 0.60f, 1.0f),
        /* BorderSoft */ ImVec4(0.19f, 0.22f, 0.26f, 1.0f),
        /* BorderStrong */ ImVec4(0.11f, 0.13f, 0.17f, 1.0f),
        /* Accent */ ImVec4(0.35f, 0.65f, 0.95f, 1.0f),
        /* AccentHover */ ImVec4(0.40f, 0.70f, 1.00f, 1.0f),
        /* AccentActive */ ImVec4(0.26f, 0.56f, 0.92f, 1.0f),
        /* Success */ ImVec4(0.38f, 0.70f, 0.35f, 1.0f),
        /* Warning */ ImVec4(0.95f, 0.67f, 0.33f, 1.0f),
        /* Danger */ ImVec4(0.95f, 0.38f, 0.34f, 1.0f),
        /* Info */ ImVec4(0.42f, 0.65f, 0.88f, 1.0f),
        /* Muted */ ImVec4(0.31f, 0.36f, 0.44f, 1.0f),
        /* Highlight */ ImVec4(0.98f, 0.82f, 0.35f, 1.0f),
    };

    constexpr SpacingArray kSpacing = {
        0.0f,  // None
        4.0f,  // Xs
        8.0f,  // Sm
        12.0f, // Md
        16.0f, // Lg
        24.0f, // Xl
    };
} // namespace

void DesignTokens::applyTo(ImGuiStyle &style)
{
    style.WindowPadding = ImVec2(spacing(SpacingToken::Lg), spacing(SpacingToken::Md));
    style.FramePadding = ImVec2(spacing(SpacingToken::Sm), spacing(SpacingToken::Xs));
    style.CellPadding = ImVec2(spacing(SpacingToken::Sm), spacing(SpacingToken::Sm));
    style.ItemSpacing = ImVec2(spacing(SpacingToken::Sm), spacing(SpacingToken::Sm));
    style.ItemInnerSpacing = ImVec2(spacing(SpacingToken::Xs), spacing(SpacingToken::Xs));
    style.IndentSpacing = spacing(SpacingToken::Lg);
    style.ScrollbarSize = scrollbarSize();

    style.WindowRounding = roundingNone();
    style.ChildRounding = roundingNone();
    style.FrameRounding = roundingNone();
    style.PopupRounding = roundingNone();
    style.ScrollbarRounding = roundingNone();
    style.GrabRounding = roundingNone();
    style.TabRounding = roundingNone();

    style.WindowBorderSize = windowBorderThickness();
    style.ChildBorderSize = windowBorderThickness();
    style.FrameBorderSize = frameBorderThickness();
    style.PopupBorderSize = windowBorderThickness();
    style.TabBorderSize = frameBorderThickness();
    style.SeparatorTextBorderSize = separatorThickness();

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
    style.Colors[ImGuiCol_NavHighlight] = color(ColorToken::PrimaryHover);
    style.Colors[ImGuiCol_NavWindowingHighlight] = color(ColorToken::PrimaryHover);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, 0.45f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.55f);
}

ImVec4 DesignTokens::color(ColorToken token)
{
    return kColors[static_cast<std::size_t>(token)];
}

float DesignTokens::spacing(SpacingToken token)
{
    return kSpacing[static_cast<std::size_t>(token)];
}

} // namespace Genesis::Sandbox::Gui::Style
