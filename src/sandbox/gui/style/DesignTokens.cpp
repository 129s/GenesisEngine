#include "sandbox/gui/style/DesignTokens.hpp"

#include <array>

namespace Genesis::Sandbox::Gui::Style
{
namespace
{
    using ColorArray = std::array<ImVec4, static_cast<std::size_t>(ColorToken::Count)>;
    using SpacingArray = std::array<float, static_cast<std::size_t>(SpacingToken::Count)>;

    const ColorArray kColors = {
        /* Canvas */ ImVec4(0.051f, 0.078f, 0.114f, 1.0f),        // #0D141D
        /* Surface */ ImVec4(0.082f, 0.125f, 0.188f, 1.0f),       // #152030
        /* SurfaceAlt */ ImVec4(0.110f, 0.169f, 0.227f, 1.0f),    // #1C2B3A
        /* SurfaceActive */ ImVec4(0.137f, 0.212f, 0.294f, 1.0f), // #23364B
        /* Primary */ ImVec4(0.196f, 0.682f, 0.937f, 1.0f),       // #32AEEF
        /* PrimaryHover */ ImVec4(0.255f, 0.741f, 0.984f, 1.0f),  // #41BDFB
        /* PrimaryActive */ ImVec4(0.118f, 0.525f, 0.816f, 1.0f), // #1E86D0
        /* TextPrimary */ ImVec4(0.922f, 0.953f, 0.992f, 1.0f),   // #EBF3FD
        /* TextSecondary */ ImVec4(0.686f, 0.769f, 0.867f, 1.0f), // #AFC4DD
        /* TextDisabled */ ImVec4(0.365f, 0.455f, 0.549f, 1.0f),  // #5D748C
        /* BorderSoft */ ImVec4(0.129f, 0.192f, 0.259f, 1.0f),    // #213142
        /* BorderStrong */ ImVec4(0.067f, 0.102f, 0.153f, 1.0f),  // #111A27
        /* Accent */ ImVec4(0.298f, 0.773f, 0.906f, 1.0f),        // #4CC5E7
        /* AccentHover */ ImVec4(0.369f, 0.824f, 0.945f, 1.0f),   // #5ED2F1
        /* AccentActive */ ImVec4(0.165f, 0.604f, 0.831f, 1.0f),  // #2A9AD4
        /* Success */ ImVec4(0.435f, 0.957f, 0.518f, 1.0f),       // #6FF484
        /* Warning */ ImVec4(1.000f, 0.784f, 0.341f, 1.0f),       // #FFC857
        /* Danger */ ImVec4(1.000f, 0.420f, 0.420f, 1.0f),        // #FF6B6B
        /* Info */ ImVec4(0.341f, 0.788f, 1.000f, 1.0f),          // #57C9FF
        /* Muted */ ImVec4(0.204f, 0.271f, 0.361f, 1.0f),         // #34455C
        /* Highlight */ ImVec4(0.608f, 0.878f, 1.000f, 1.0f),     // #9BE0FF
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
    return kColors[static_cast<std::size_t>(token)];
}

float DesignTokens::spacing(SpacingToken token)
{
    return kSpacing[static_cast<std::size_t>(token)];
}

} // namespace Genesis::Sandbox::Gui::Style
