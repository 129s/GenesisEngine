#pragma once

#include <array>

#include <imgui.h>

namespace Genesis::Sandbox::Gui::Style
{

enum class ColorToken : std::size_t
{
    Canvas = 0,
    Surface,
    SurfaceAlt,
    SurfaceActive,
    Primary,
    PrimaryHover,
    PrimaryActive,
    TextPrimary,
    TextSecondary,
    TextDisabled,
    BorderSoft,
    BorderStrong,
    Accent,
    AccentHover,
    AccentActive,
    Success,
    Warning,
    Danger,
    Info,
    Muted,
    Highlight,
    Count
};

enum class SpacingToken : std::size_t
{
    None = 0,
    Xs,
    Sm,
    Md,
    Lg,
    Xl,
    Count
};

struct DesignTokens
{
    static void applyTo(ImGuiStyle &style);

    static ImVec4 color(ColorToken token);
    static float spacing(SpacingToken token);

    static constexpr float frameBorderThickness() { return 1.0f; }
    static constexpr float windowBorderThickness() { return 1.0f; }
    static constexpr float separatorThickness() { return 1.0f; }
    static constexpr float dockingSeparatorThickness() { return 4.0f; }

    static constexpr float roundingNone() { return 0.0f; }
    static constexpr float scrollbarSize() { return 16.0f; }
};

} // namespace Genesis::Sandbox::Gui::Style
