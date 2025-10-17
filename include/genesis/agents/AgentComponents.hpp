#pragma once

#include <vector>

#include "genesis/world/WorldTypes.hpp"

namespace genesis::agents::components {

struct AgentLocation {
    genesis::world::LocationId location{genesis::world::InvalidLocation};
};

struct MovementIntent {
    genesis::world::LocationId target{genesis::world::InvalidLocation};
    float speed{1.0f};
};

struct MovementState {
    genesis::world::LocationId destination{genesis::world::InvalidLocation};
    std::vector<genesis::world::LocationId> path;
    std::size_t currentIndex{0};
    float distanceRemaining{0.0f};
    std::vector<float> accumulatedDistances;
    float traveledAlongEdge{0.0f};
    bool blocked{false};
};

} // namespace genesis::agents::components

