#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Genesis::Style
{

enum class ColorSpace
{
    SRGB,
    Linear
};

struct RgbaColor
{
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{1.0f};
};

struct ColorSample
{
    RgbaColor srgb{};
    RgbaColor linear{};

    [[nodiscard]] RgbaColor value(ColorSpace space) const noexcept
    {
        return space == ColorSpace::Linear ? linear : srgb;
    }
};

struct PaletteEntry
{
    std::string token;
    std::string category;
    std::string description;
    ColorSample sample{};
    std::vector<std::string> aliases;
};

struct PaletteTheme
{
    std::string id;
    std::string label;
    std::string description;
    std::unordered_map<std::string, PaletteEntry> entries; // keyed by canonical token
    std::unordered_map<std::string, std::string> alias_map; // alias -> canonical token
};

struct PaletteDocument
{
    int version{1};
    std::string default_theme_id;
    std::unordered_map<std::string, PaletteTheme> themes;
};

} // namespace Genesis::Style

