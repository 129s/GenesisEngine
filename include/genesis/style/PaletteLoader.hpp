#pragma once

#include "genesis/style/PaletteTypes.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace Genesis::Style
{

struct PaletteLoadResult
{
    bool ok{false};
    std::string error;
};

PaletteLoadResult LoadPaletteDocument(const std::filesystem::path& filePath, PaletteDocument& document);

} // namespace Genesis::Style

