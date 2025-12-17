#pragma once

#include <cstdint>

#include "genesis/world/Namespace.hpp"

namespace genesis::world {

// 资源类型：保留为运行时系统共享的最小枚举。
enum class ResourceType : std::uint8_t {
    Food,
    Water,
    Social,
    Ore,
    Tool
};

} // namespace genesis::world
