#include "genesis/style/ColorRegistry.hpp"

#include "genesis/style/PaletteLoader.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace Genesis::Style
{
ColorRegistry& ColorRegistry::instance()
{
    static ColorRegistry registry;
    return registry;
}

bool ColorRegistry::isLoaded() const noexcept
{
    std::scoped_lock lock(mutex_);
    return loaded_;
}

bool ColorRegistry::tryLoadFrom(const std::filesystem::path& filePath, std::optional<std::string> forcedTheme)
{
    return loadInternal(filePath, std::move(forcedTheme));
}

void ColorRegistry::unload()
{
    std::scoped_lock lock(mutex_);
    document_ = PaletteDocument{};
    active_theme_.clear();
    source_path_.clear();
    version_ = 0;
    loaded_ = false;
}

std::optional<RgbaColor> ColorRegistry::color(std::string_view token,
                                              ColorSpace space,
                                              std::optional<std::string_view> theme) const
{
    std::scoped_lock lock(mutex_);
    if (!loaded_)
    {
        return std::nullopt;
    }

    const PaletteTheme* themePtr = nullptr;
    if (theme && !theme->empty())
    {
        themePtr = findTheme(*theme);
    }
    if (!themePtr)
    {
        themePtr = findTheme(active_theme_);
    }
    if (!themePtr)
    {
        return std::nullopt;
    }

    return fetchFromTheme(*themePtr, token, space);
}

std::vector<std::string> ColorRegistry::themes() const
{
    std::scoped_lock lock(mutex_);
    std::vector<std::string> result;
    result.reserve(document_.themes.size());
    for (const auto& [id, _] : document_.themes)
    {
        result.push_back(id);
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> ColorRegistry::tokens(std::optional<std::string_view> theme) const
{
    std::scoped_lock lock(mutex_);
    const PaletteTheme* themePtr = nullptr;
    if (theme && !theme->empty())
    {
        themePtr = findTheme(*theme);
    }
    if (!themePtr)
    {
        themePtr = findTheme(active_theme_);
    }
    std::vector<std::string> result;
    if (!themePtr)
    {
        return result;
    }
    result.reserve(themePtr->entries.size());
    for (const auto& [tokenName, _] : themePtr->entries)
    {
        result.push_back(tokenName);
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

const std::string& ColorRegistry::defaultTheme() const noexcept
{
    return document_.default_theme_id;
}

const std::string& ColorRegistry::activeTheme() const noexcept
{
    return active_theme_;
}

bool ColorRegistry::setActiveTheme(std::string_view themeId)
{
    std::scoped_lock lock(mutex_);
    if (!loaded_)
    {
        return false;
    }
    if (!findTheme(themeId))
    {
        return false;
    }
    active_theme_ = std::string(themeId);
    return true;
}

int ColorRegistry::version() const noexcept
{
    std::scoped_lock lock(mutex_);
    return version_;
}

const std::filesystem::path& ColorRegistry::sourcePath() const noexcept
{
    return source_path_;
}

bool ColorRegistry::loadInternal(const std::filesystem::path& filePath, std::optional<std::string> forcedTheme)
{
    PaletteDocument document{};
    const auto result = LoadPaletteDocument(filePath, document);
    if (!result.ok)
    {
        spdlog::error("ColorRegistry: 加载调色板失败 {} -> {}", filePath.string(), result.error);
        return false;
    }

    std::string parsedActiveTheme = forcedTheme.value_or(document.default_theme_id);
    if (!document.themes.count(parsedActiveTheme))
    {
        spdlog::warn("ColorRegistry: 指定的主题 {} 不存在，将使用默认主题 {}", parsedActiveTheme, document.default_theme_id);
        parsedActiveTheme = document.default_theme_id;
    }

    {
        std::scoped_lock lock(mutex_);
        document_ = std::move(document);
        active_theme_ = std::move(parsedActiveTheme);
        source_path_ = filePath;
        version_ = document_.version;
        loaded_ = true;
    }

    spdlog::info("ColorRegistry: 已加载调色板 {}, version={}, active_theme={}", filePath.string(), version_, active_theme_);
    return true;
}

const PaletteTheme* ColorRegistry::findTheme(std::string_view themeId) const
{
    if (themeId.empty())
    {
        return nullptr;
    }
    const auto it = document_.themes.find(std::string(themeId));
    if (it == document_.themes.end())
    {
        return nullptr;
    }
    return &it->second;
}

std::optional<RgbaColor> ColorRegistry::fetchFromTheme(const PaletteTheme& theme,
                                                       std::string_view token,
                                                       ColorSpace space) const
{
    const PaletteEntry* entry = resolveEntry(theme, token);
    if (!entry)
    {
        return std::nullopt;
    }
    return entry->sample.value(space);
}

const PaletteEntry* ColorRegistry::resolveEntry(const PaletteTheme& theme, std::string_view token) const
{
    const auto canonical = theme.entries.find(std::string(token));
    if (canonical != theme.entries.end())
    {
        return &canonical->second;
    }
    const auto aliasIt = theme.alias_map.find(std::string(token));
    if (aliasIt == theme.alias_map.end())
    {
        return nullptr;
    }
    const auto linked = theme.entries.find(aliasIt->second);
    if (linked == theme.entries.end())
    {
        return nullptr;
    }
    return &linked->second;
}

} // namespace Genesis::Style
