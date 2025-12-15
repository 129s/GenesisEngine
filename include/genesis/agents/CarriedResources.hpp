#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "genesis/world/WorldTypes.hpp"

namespace genesis::agents::components {

constexpr std::size_t resourceTypeIndex(genesis::world::ResourceType type) noexcept {
    switch (type) {
    case genesis::world::ResourceType::Food:
        return 0;
    case genesis::world::ResourceType::Water:
        return 1;
    case genesis::world::ResourceType::Social:
        return 2;
    case genesis::world::ResourceType::Ore:
        return 3;
    case genesis::world::ResourceType::Tool:
        return 4;
    }
    return 0;
}

struct CarriedResources {
    static constexpr std::size_t kTypeCount = 5;

    std::array<std::uint32_t, kTypeCount> units{};
    std::uint32_t capacityPerType{12};

    [[nodiscard]] std::uint32_t get(genesis::world::ResourceType type) const noexcept {
        return units[resourceTypeIndex(type)];
    }

    [[nodiscard]] std::uint32_t remaining(genesis::world::ResourceType type) const noexcept {
        const auto have = get(type);
        if (have >= capacityPerType) {
            return 0;
        }
        return capacityPerType - have;
    }

    std::uint32_t add(genesis::world::ResourceType type, std::uint32_t amount) noexcept {
        auto& slot = units[resourceTypeIndex(type)];
        const auto space = (slot >= capacityPerType) ? 0U : (capacityPerType - slot);
        const auto taken = (amount <= space) ? amount : space;
        slot += taken;
        return taken;
    }

    std::uint32_t remove(genesis::world::ResourceType type, std::uint32_t amount) noexcept {
        auto& slot = units[resourceTypeIndex(type)];
        const auto taken = (amount <= slot) ? amount : slot;
        slot -= taken;
        return taken;
    }
};

} // namespace genesis::agents::components
