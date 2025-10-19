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

struct WorldSaveResult {
    bool success{false};
    std::string error;
};

struct WorldGraphLoadResult {
    bool success{false};
    std::string error;
    LocationGraph graph;
};

WorldGraphLoadResult loadWorldGraphFromFile(const std::filesystem::path& path);
WorldGraphLoadResult loadWorldGraphFromJsonString(std::string_view jsonData);

WorldLoadResult loadWorldFromFile(const std::filesystem::path& path, WorldRegistry& registry);

WorldLoadResult loadWorldFromJsonString(std::string_view jsonData, WorldRegistry& registry);

WorldSaveResult saveWorldToFile(const std::filesystem::path& path, const LocationGraph& graph);

} // namespace genesis::world
