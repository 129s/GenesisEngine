#include "genesis/style/ColorRegistry.hpp"

#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <sstream>
#include <type_traits>

namespace Genesis::Style
{
namespace
{
    using json = nlohmann::json;

    constexpr float Clamp(float value, float min, float max)
    {
        return value < min ? min : (value > max ? max : value);
    }

    float SrgbChannelToLinear(float channel)
    {
        if (channel <= 0.04045f)
        {
            return channel / 12.92f;
        }
        return std::pow((channel + 0.055f) / 1.055f, 2.4f);
    }

    RgbaColor SrgbToLinear(const RgbaColor& srgb)
    {
        return {SrgbChannelToLinear(srgb.r),
                SrgbChannelToLinear(srgb.g),
                SrgbChannelToLinear(srgb.b),
                srgb.a};
    }

    std::optional<int> ParseHexComponent(std::string_view text)
    {
        unsigned int value = 0;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value, 16);
        if (result.ec != std::errc{})
        {
            return std::nullopt;
        }
        return static_cast<int>(value);
    }

    std::optional<RgbaColor> ParseHexColor(std::string_view text)
    {
        if (text.empty())
        {
            return std::nullopt;
        }
        if (text.front() == '#')
        {
            text.remove_prefix(1);
        }

        auto normalize = [](int component) -> float
        {
            return Clamp(static_cast<float>(component) / 255.0f, 0.0f, 1.0f);
        };

        RgbaColor color{};

        if (text.size() == 6u || text.size() == 8u)
        {
            const auto r = ParseHexComponent(text.substr(0, 2));
            const auto g = ParseHexComponent(text.substr(2, 2));
            const auto b = ParseHexComponent(text.substr(4, 2));
            if (!r || !g || !b)
            {
                return std::nullopt;
            }

            color.r = normalize(*r);
            color.g = normalize(*g);
            color.b = normalize(*b);
            color.a = 1.0f;

            if (text.size() == 8u)
            {
                const auto a = ParseHexComponent(text.substr(6, 2));
                if (!a)
                {
                    return std::nullopt;
                }
                color.a = normalize(*a);
            }
            return color;
        }

        if (text.size() == 3u || text.size() == 4u)
        {
            const auto expand = [](char c) -> std::array<char, 2>
            {
                return {c, c};
            };

            std::array<char, 8> expanded{};
            auto it = expanded.begin();
            for (std::size_t index = 0; index < text.size(); ++index)
            {
                const auto pair = expand(text[index]);
                *it++ = pair[0];
                *it++ = pair[1];
            }
            std::string expandedText(expanded.begin(), expanded.begin() + static_cast<std::ptrdiff_t>(text.size() * 2));
            return ParseHexColor(expandedText);
        }

        return std::nullopt;
    }

    std::optional<RgbaColor> ParseColorNode(const json& value)
    {
        if (value.is_string())
        {
            return ParseHexColor(value.get<std::string>());
        }
        if (value.is_array())
        {
            if (value.empty() || value.size() < 3 || value.size() > 4)
            {
                return std::nullopt;
            }
            RgbaColor color{};
            color.r = Clamp(value.at(0).get<float>(), 0.0f, 1.0f);
            color.g = Clamp(value.at(1).get<float>(), 0.0f, 1.0f);
            color.b = Clamp(value.at(2).get<float>(), 0.0f, 1.0f);
            color.a = value.size() == 4 ? Clamp(value.at(3).get<float>(), 0.0f, 1.0f) : 1.0f;
            return color;
        }
        if (value.is_object())
        {
            if (!value.contains("r") || !value.contains("g") || !value.contains("b"))
            {
                return std::nullopt;
            }
            RgbaColor color{};
            color.r = Clamp(value.at("r").get<float>(), 0.0f, 1.0f);
            color.g = Clamp(value.at("g").get<float>(), 0.0f, 1.0f);
            color.b = Clamp(value.at("b").get<float>(), 0.0f, 1.0f);
            color.a = value.contains("a") ? Clamp(value.at("a").get<float>(), 0.0f, 1.0f) : 1.0f;
            return color;
        }
        return std::nullopt;
    }

    PaletteEntry BuildEntry(const std::string& tokenName, const json& node)
    {
        PaletteEntry entry{};
        entry.name = tokenName;
        entry.category = node.value("category", std::string{});
        entry.description = node.value("description", std::string{});

        if (node.contains("srgb"))
        {
            if (const auto srgb = ParseColorNode(node.at("srgb")))
            {
                entry.sample.srgb = *srgb;
            }
        }

        if (node.contains("linear"))
        {
            if (const auto linear = ParseColorNode(node.at("linear")))
            {
                entry.sample.linear = *linear;
            }
        }
        else
        {
            entry.sample.linear = SrgbToLinear(entry.sample.srgb);
        }

        return entry;
    }

    void RegisterEntryWithAliases(PaletteTheme& theme, const PaletteEntry& base, const json& node)
    {
        theme.entries[base.name] = base;
        const auto aliasesIt = node.find("aliases");
        if (aliasesIt == node.end() || !aliasesIt->is_array())
        {
            return;
        }
        for (const auto& aliasNode : *aliasesIt)
        {
            if (!aliasNode.is_string())
            {
                continue;
            }
            PaletteEntry aliasEntry = base;
            aliasEntry.name = aliasNode.get<std::string>();
            theme.entries[aliasEntry.name] = aliasEntry;
        }
    }
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

void ColorRegistry::unload()
{
    std::scoped_lock lock(mutex_);
    themes_.clear();
    default_theme_.clear();
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
    result.reserve(themes_.size());
    for (const auto& [id, _] : themes_)
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
    return default_theme_;
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
    std::ifstream input(filePath);
    if (!input.is_open())
    {
        spdlog::error("ColorRegistry: 无法打开调色板文件 {}", filePath.string());
        return false;
    }

    json document;
    try
    {
        document = json::parse(input, nullptr, true, true);
    }
    catch (const json::parse_error& error)
    {
        spdlog::error("ColorRegistry: 解析 JSON 失败 ({}): {}", filePath.string(), error.what());
        return false;
    }

    if (!document.contains("themes") || !document.at("themes").is_array())
    {
        spdlog::error("ColorRegistry: 配置缺少 themes 列表 ({})", filePath.string());
        return false;
    }

    std::unordered_map<std::string, PaletteTheme> parsedThemes;
    for (const auto& themeNode : document.at("themes"))
    {
        if (!themeNode.contains("id"))
        {
            spdlog::warn("ColorRegistry: theme 节点缺少 id，已跳过");
            continue;
        }
        PaletteTheme theme;
        theme.id = themeNode.at("id").get<std::string>();
        theme.label = themeNode.value("label", theme.id);
        theme.description = themeNode.value("description", std::string{});

        const auto& tokensNode = themeNode.at("tokens");
        if (!tokensNode.is_object())
        {
            spdlog::warn("ColorRegistry: theme {} 的 tokens 不是对象，已跳过", theme.id);
            continue;
        }

        for (auto it = tokensNode.begin(); it != tokensNode.end(); ++it)
        {
            const std::string tokenName = it.key();
            const json& tokenNode = it.value();
            PaletteEntry entry = BuildEntry(tokenName, tokenNode);
            RegisterEntryWithAliases(theme, entry, tokenNode);
        }

        if (!theme.entries.empty())
        {
            parsedThemes.emplace(theme.id, std::move(theme));
        }
        else
        {
            spdlog::warn("ColorRegistry: theme {} 未包含任何 token，已忽略", theme.id);
        }
    }

    if (parsedThemes.empty())
    {
        spdlog::error("ColorRegistry: 未解析到有效主题 ({})", filePath.string());
        return false;
    }

    const int parsedVersion = document.value("version", 1);
    std::string parsedDefaultTheme = document.value("default_theme", parsedThemes.begin()->first);
    if (!parsedThemes.count(parsedDefaultTheme))
    {
        spdlog::warn("ColorRegistry: default_theme 指向不存在的主题 {}，将使用 {}", parsedDefaultTheme, parsedThemes.begin()->first);
        parsedDefaultTheme = parsedThemes.begin()->first;
    }

    std::string parsedActiveTheme = forcedTheme.value_or(parsedDefaultTheme);
    if (!parsedThemes.count(parsedActiveTheme))
    {
        spdlog::warn("ColorRegistry: 指定的主题 {} 不存在，将使用默认主题 {}", parsedActiveTheme, parsedDefaultTheme);
        parsedActiveTheme = parsedDefaultTheme;
    }

    {
        std::scoped_lock lock(mutex_);
        themes_ = std::move(parsedThemes);
        default_theme_ = std::move(parsedDefaultTheme);
        active_theme_ = std::move(parsedActiveTheme);
        source_path_ = filePath;
        version_ = parsedVersion;
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
    const auto it = themes_.find(std::string(themeId));
    if (it == themes_.end())
    {
        return nullptr;
    }
    return &it->second;
}

std::optional<RgbaColor> ColorRegistry::fetchFromTheme(const PaletteTheme& theme,
                                                       std::string_view token,
                                                       ColorSpace space) const
{
    const auto it = theme.entries.find(std::string(token));
    if (it == theme.entries.end())
    {
        return std::nullopt;
    }
    return it->second.sample.value(space);
}

} // namespace Genesis::Style

