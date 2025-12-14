#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "genesis/world/WorldDatabase.hpp"

namespace genesis::world {

struct MapPath {
    std::vector<MapId> maps; // includes start and target
    double totalCost{0.0};
};

[[nodiscard]] std::optional<MapPath> shortestMapPath(const WorldDatabase& db, MapId start, MapId target);

} // namespace genesis::world

