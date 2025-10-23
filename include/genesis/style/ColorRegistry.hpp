#pragma once

#include "genesis/style/PaletteTypes.hpp"

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Genesis::Style
{

class ColorRegistry
{
public:
    static ColorRegistry& instance();

    [[nodiscard]] bool isLoaded() const noexcept;

    bool tryLoadFrom(const std::filesystem::path& filePath, std::optional<std::string> forcedTheme = std::nullopt);
    bool loadCompiledDefault(std::optional<std::string> forcedTheme = std::nullopt);
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
    const PaletteEntry* resolveEntry(const PaletteTheme& theme, std::string_view token) const;

    mutable std::mutex mutex_;
    PaletteDocument document_;
    std::string active_theme_;
    std::filesystem::path source_path_;
    int version_{0};
    bool loaded_{false};
};

} // namespace Genesis::Style
