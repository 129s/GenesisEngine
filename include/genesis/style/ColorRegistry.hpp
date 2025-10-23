#pragma once

#include <filesystem>
#include <mutex>
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
    ColorSample sample{};
    std::string name;
    std::string category;
    std::string description;
};

struct PaletteTheme
{
    std::string id;
    std::string label;
    std::string description;
    std::unordered_map<std::string, PaletteEntry> entries;
};

class ColorRegistry
{
public:
    static ColorRegistry& instance();

    [[nodiscard]] bool isLoaded() const noexcept;

    bool tryLoadFrom(const std::filesystem::path& filePath, std::optional<std::string> forcedTheme = std::nullopt);
    void unload();

    [[nodiscard]] std::optional<RgbaColor> color(std::string_view token,
                                                 ColorSpace space = ColorSpace::SRGB,
                                                 std::optional<std::string_view> theme = std::nullopt) const;

    [[nodiscard]] std::vector<std::string> themes() const;
    [[nodiscard]] std::vector<std::string> tokens(std::optional<std::string_view> theme = std::nullopt) const;

    [[nodiscard]] const std::string& defaultTheme() const noexcept;
    [[nodiscard]] const std::string& activeTheme() const noexcept;
    bool setActiveTheme(std::string_view themeId);

    [[nodiscard]] int version() const noexcept;

    [[nodiscard]] const std::filesystem::path& sourcePath() const noexcept;

private:
    ColorRegistry() = default;

    bool loadInternal(const std::filesystem::path& filePath, std::optional<std::string> forcedTheme);
    const PaletteTheme* findTheme(std::string_view themeId) const;
    std::optional<RgbaColor> fetchFromTheme(const PaletteTheme& theme, std::string_view token, ColorSpace space) const;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, PaletteTheme> themes_;
    std::string default_theme_;
    std::string active_theme_;
    std::filesystem::path source_path_;
    int version_{0};
    bool loaded_{false};
};

} // namespace Genesis::Style

