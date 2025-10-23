#include "genesis/style/PaletteLoader.hpp"

#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <sstream>

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

    PaletteEntry BuildEntry(const std::string& tokenName, const json& node, std::string& outError)
    {
        PaletteEntry entry{};
        entry.token = tokenName;
        entry.category = node.value("category", std::string{});
        entry.description = node.value("description", std::string{});

        if (node.contains("srgb"))
        {
            if (const auto srgb = ParseColorNode(node.at("srgb")))
            {
                entry.sample.srgb = *srgb;
            }
            else
            {
                outError = "无法解析 token '" + tokenName + "' 的 srgb 色值";
            }
        }
        else
        {
            outError = "token '" + tokenName + "' 缺少 srgb 定义";
        }

        if (!outError.empty())
        {
            return entry;
        }

        if (node.contains("linear"))
        {
            if (const auto linear = ParseColorNode(node.at("linear")))
            {
                entry.sample.linear = *linear;
            }
            else
            {
                outError = "无法解析 token '" + tokenName + "' 的 linear 色值";
                return entry;
            }
        }
        else
        {
            entry.sample.linear = SrgbToLinear(entry.sample.srgb);
        }

        const auto aliasesIt = node.find("aliases");
        if (aliasesIt != node.end() && aliasesIt->is_array())
        {
            for (const auto& aliasNode : *aliasesIt)
            {
                if (aliasNode.is_string())
                {
                    entry.aliases.push_back(aliasNode.get<std::string>());
                }
            }
        }

        return entry;
    }
} // namespace

PaletteLoadResult LoadPaletteDocument(const std::filesystem::path& filePath, PaletteDocument& document)
{
    PaletteLoadResult result{};
    std::ifstream input(filePath);
    if (!input.is_open())
    {
        result.error = "无法打开文件";
        return result;
    }

    json parsed;
    try
    {
        parsed = json::parse(input, nullptr, true, true);
    }
    catch (const json::parse_error& error)
    {
        result.error = error.what();
        return result;
    }

    if (!parsed.contains("themes") || !parsed.at("themes").is_array())
    {
        result.error = "缺少 themes 数组";
        return result;
    }

    PaletteDocument loaded{};
    loaded.version = parsed.value("version", 1);

    for (const auto& themeNode : parsed.at("themes"))
    {
        if (!themeNode.contains("id"))
        {
            spdlog::warn("PaletteLoader: theme 节点缺少 id，已跳过");
            continue;
        }

        PaletteTheme theme{};
        theme.id = themeNode.at("id").get<std::string>();
        theme.label = themeNode.value("label", theme.id);
        theme.description = themeNode.value("description", std::string{});

        const auto tokensIt = themeNode.find("tokens");
        if (tokensIt == themeNode.end() || !tokensIt->is_object())
        {
            spdlog::warn("PaletteLoader: theme {} 的 tokens 不是对象，已忽略", theme.id);
            continue;
        }

        bool failed = false;
        for (auto it = tokensIt->begin(); it != tokensIt->end(); ++it)
        {
            std::string error;
            PaletteEntry entry = BuildEntry(it.key(), it.value(), error);
            if (!error.empty())
            {
                spdlog::error("PaletteLoader: {} -> {}", theme.id, error);
                failed = true;
                break;
            }

            if (theme.entries.count(entry.token))
            {
                spdlog::error("PaletteLoader: theme {} 存在重复 token {}", theme.id, entry.token);
                failed = true;
                break;
            }

            for (const auto& alias : entry.aliases)
            {
                if (theme.alias_map.count(alias) > 0 || theme.entries.count(alias) > 0)
                {
                    spdlog::error("PaletteLoader: theme {} 中 alias '{}' 与已有 token/alias 冲突", theme.id, alias);
                    failed = true;
                    break;
                }
                theme.alias_map.emplace(alias, entry.token);
            }
            if (failed)
            {
                break;
            }

            theme.entries.emplace(entry.token, std::move(entry));
        }

        if (failed)
        {
            result.error = "theme '" + theme.id + "' 解析失败";
            return result;
        }

        if (!theme.entries.empty())
        {
            loaded.themes.emplace(theme.id, std::move(theme));
        }
    }

    if (loaded.themes.empty())
    {
        result.error = "未解析到任何有效 theme";
        return result;
    }

    loaded.default_theme_id = parsed.value("default_theme", loaded.themes.begin()->first);
    if (!loaded.themes.count(loaded.default_theme_id))
    {
        spdlog::warn("PaletteLoader: default_theme 指向不存在的主题 {}，改为 {}", loaded.default_theme_id, loaded.themes.begin()->first);
        loaded.default_theme_id = loaded.themes.begin()->first;
    }

    document = std::move(loaded);
    result.ok = true;
    return result;
}

} // namespace Genesis::Style

