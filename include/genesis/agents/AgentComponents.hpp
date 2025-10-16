#pragma once

#include "genesis/world/WorldTypes.hpp"

namespace genesis::agents::components {

struct AgentLocation {
    genesis::world::LocationId location{genesis::world::InvalidLocation};
};

} // namespace genesis::agents::components

