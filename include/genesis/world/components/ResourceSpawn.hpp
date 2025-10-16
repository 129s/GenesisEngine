#pragma once

#include <cstdint>
#include <string>

#include "genesis/world/WorldTypes.hpp"

namespace genesis::world::components {

struct ResourceSpawn {
    std::string name;
    ::genesis::world::ResourceType type{::genesis::world::ResourceType::Food};
    ::genesis::world::LocationId location{::genesis::world::InvalidLocation};
    std::uint32_t ratePerStep{0};
};

} // namespace genesis::world::components

