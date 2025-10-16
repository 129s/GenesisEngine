#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "genesis/world/WorldRegistry.hpp"

namespace genesis::world {

struct WorldLoadResult {
    bool success{false};
    std::string error;
};

WorldLoadResult loadWorldFromFile(const std::filesystem::path& path, WorldRegistry& registry);

WorldLoadResult loadWorldFromJsonString(std::string_view jsonData, WorldRegistry& registry);

} // namespace genesis::world
