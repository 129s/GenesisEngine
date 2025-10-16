#pragma once

#include <cstdint>
#include <string>

#include "genesis/world/WorldTypes.hpp"

namespace genesis::world::events {

struct ResourceConsumed {
    std::string name;
    genesis::world::ResourceType type{genesis::world::ResourceType::Food};
    genesis::world::LocationId location{genesis::world::InvalidLocation};
    std::uint32_t amount{0};
    std::uint32_t remaining{0};
};

struct ResourceLowStock {
    std::string name;
    genesis::world::ResourceType type{genesis::world::ResourceType::Food};
    genesis::world::LocationId location{genesis::world::InvalidLocation};
    std::uint32_t remaining{0};
    std::uint32_t capacity{0};
};

} // namespace genesis::world::events

