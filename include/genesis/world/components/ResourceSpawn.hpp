#pragma once

#include <cstdint>
#include <string>

#include "genesis/world/WorldTypes.hpp"
#include "genesis/world/WorldDatabase.hpp"

namespace genesis::world::components {

struct ResourceSpawn {
    std::string name;
    ::genesis::world::ResourceType type{::genesis::world::ResourceType::Food};
    ::genesis::world::InteractionId interaction{0};
    ::genesis::world::MapId mapId{0};
    std::uint32_t ratePerStep{0};
    std::uint32_t decayPerStep{0};
};

} // namespace genesis::world::components

