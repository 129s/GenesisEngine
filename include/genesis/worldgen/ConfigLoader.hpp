#pragma once

#include <filesystem>

#include "genesis/worldgen/Types.hpp"

namespace genesis::worldgen
{

GeneratorConfig load_config(const std::filesystem::path& path);

} // namespace genesis::worldgen

