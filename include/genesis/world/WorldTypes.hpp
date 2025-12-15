#pragma once

#include <cstdint>

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

namespace Genesis {
namespace World = genesis::world;
} // namespace Genesis
