#pragma once

#include <cstdint>

namespace genesis::world::components {

struct ResourceInventory {
    std::uint32_t current{0};
    std::uint32_t capacity{0};
};

} // namespace genesis::world::components

