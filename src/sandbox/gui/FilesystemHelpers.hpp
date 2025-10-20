#pragma once

#include <array>
#include <cstdlib>
#include <filesystem>

namespace Genesis::Sandbox::Gui
{

inline std::filesystem::path locateAsset(const std::filesystem::path& relative)
{
    constexpr int searchDepth = 5;
    auto current = std::filesystem::current_path();
    for (int i = 0; i <= searchDepth; ++i)
    {
        const auto candidate = current / relative;
        if (std::filesystem::exists(candidate))
        {
            return candidate;
        }
        if (!current.has_parent_path())
        {
            break;
        }
        current = current.parent_path();
    }
    return {};
}

inline std::filesystem::path locateCjkFont()
{
    if (auto font = locateAsset(std::filesystem::path("data/fonts/NotoSansSC-Regular.ttf")); !font.empty())
    {
        return font;
    }

#if defined(_WIN32)
    if (const char* winDir = std::getenv("WINDIR"); winDir && winDir[0] != '\0')
    {
        const std::filesystem::path base{winDir};
        const std::array<const char*, 3> candidates = {"simsun.ttc", "simhei.ttf", "msyh.ttc"};
        for (const auto* name : candidates)
        {
            const auto systemFont = base / "Fonts" / name;
            if (std::filesystem::exists(systemFont))
            {
                return systemFont;
            }
        }
    }
#endif

    return {};
}

} // namespace Genesis::Sandbox::Gui

