#include "genesis/style/ColorRegistry.hpp"

#include "genesis/style/PaletteLoader.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <optional>
#include <string>

#if __has_include("palette/PaletteGenerated.hpp")
#    include "palette/PaletteGenerated.hpp"
#    define GENESIS_STYLE_HAS_GENERATED_PALETTE 1
#else
#    define GENESIS_STYLE_HAS_GENERATED_PALETTE 0
#endif

namespace Genesis::Style
{
namespace
{
#if GENESIS_STYLE_HAS_GENERATED_PALETTE
    std::optional<PaletteDocument> BuildDocumentFromGenerated()
    {
        PaletteDocument document{};
        document.version = Generated::PaletteVersion;
        if (Generated::DefaultThemeId != nullptr)
        {
            document.default_theme_id = Generated::DefaultThemeId;
        }

        for (const auto& themeInfo : Generated::Themes)
        {
            if (themeInfo.id == nullptr)
            {
                spdlog::warn("ColorRegistry: 编译期调色板存在缺少 id 的主题，已跳过");
                continue;
            }

            PaletteTheme theme;
            theme.id = themeInfo.id;
            if (themeInfo.label != nullptr)
            {
                theme.label = themeInfo.label;
            }
            if (themeInfo.description != nullptr)
            {
                theme.description = themeInfo.description;
            }

            for (std::size_t entryIndex = 0; entryIndex < themeInfo.count; ++entryIndex)
            {
                const auto& entryInfo = themeInfo.entries[entryIndex];
                if (entryInfo.token == nullptr)
                {
                    spdlog::warn("ColorRegistry: 主题 {} 存在空 token，已忽略该条目", theme.id);
                    continue;
                }

                PaletteEntry entry;
                entry.token = entryInfo.token;
                if (entryInfo.category != nullptr)
                {
                    entry.category = entryInfo.category;
                }
                if (entryInfo.description != nullptr)
                {
                    entry.description = entryInfo.description;
                }
                entry.sample.srgb = RgbaColor{
                    entryInfo.color.srgb[0],
                    entryInfo.color.srgb[1],
                    entryInfo.color.srgb[2],
                    entryInfo.color.srgb[3],
                };
                entry.sample.linear = RgbaColor{
                    entryInfo.color.linear[0],
                    entryInfo.color.linear[1],
                    entryInfo.color.linear[2],
                    entryInfo.color.linear[3],
                };

                if (entryInfo.alias_count > 0 && entryInfo.aliases != nullptr)
                {
                    entry.aliases.reserve(entryInfo.alias_count);
                    for (std::size_t aliasIndex = 0; aliasIndex < entryInfo.alias_count; ++aliasIndex)
                    {
                        const char* alias = entryInfo.aliases[aliasIndex];
                        if (alias == nullptr || std::string_view(alias).empty())
                        {
                            continue;
                        }
                        entry.aliases.emplace_back(alias);
                    }
                }

                if (theme.entries.count(entry.token) != 0)
                {
                    spdlog::error("ColorRegistry: 主题 {} 存在重复 token {}", theme.id, entry.token);
                    return std::nullopt;
                }

                theme.entries.emplace(entry.token, entry);
                for (const auto& alias : entry.aliases)
                {
                    theme.alias_map.emplace(alias, entry.token);
                }
            }

            if (!theme.entries.empty())
            {
                theme.alias_map.reserve(theme.entries.size());
                document.themes.emplace(theme.id, std::move(theme));
            }
        }

        if (document.themes.empty())
        {
            return std::nullopt;
        }

        if (document.default_theme_id.empty() || !document.themes.count(document.default_theme_id))
        {
            document.default_theme_id = document.themes.begin()->first;
        }

        return document;
    }
#endif
} // namespace

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

bool ColorRegistry::loadCompiledDefault(std::optional<std::string> forcedTheme)
{
#if GENESIS_STYLE_HAS_GENERATED_PALETTE
    auto documentOpt = BuildDocumentFromGenerated();
    if (!documentOpt)
    {
        spdlog::error("ColorRegistry: 编译期调色板数据不可用");
        return false;
    }

    PaletteDocument document = std::move(*documentOpt);
    std::string activeTheme = forcedTheme.value_or(document.default_theme_id);
    if (!document.themes.count(activeTheme))
    {
        spdlog::warn("ColorRegistry: 编译期主题 {} 不存在，将使用默认主题 {}", activeTheme, document.default_theme_id);
        activeTheme = document.default_theme_id;
    }

    {
        std::scoped_lock lock(mutex_);
        document_ = std::move(document);
        active_theme_ = std::move(activeTheme);
        source_path_.clear();
        version_ = document_.version;
        loaded_ = true;
    }

    spdlog::info("ColorRegistry: 使用编译期调色板 version={}, active_theme={}", version_, active_theme_);
    return true;
#else
    (void)forcedTheme;
    spdlog::warn("ColorRegistry: 未找到编译期调色板头文件，无法使用内置数据");
    return false;
#endif
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
