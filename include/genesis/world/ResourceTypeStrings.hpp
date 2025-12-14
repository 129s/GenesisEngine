#pragma once

#include <optional>
#include <string_view>

#include "genesis/world/WorldTypes.hpp"

namespace genesis::world {

[[nodiscard]] constexpr const char* resourceTypeName(ResourceType type) noexcept {
    switch (type) {
    case ResourceType::Food:
        return "Food";
    case ResourceType::Water:
        return "Water";
    case ResourceType::Social:
        return "Social";
    }
    return "Food";
}

[[nodiscard]] constexpr std::optional<ResourceType> parseResourceType(std::string_view sv) noexcept {
    // Canonical + lower-case + legacy alias (Drink -> Water)
    if (sv == "Food" || sv == "food") return ResourceType::Food;
    if (sv == "Water" || sv == "water" || sv == "Drink" || sv == "drink") return ResourceType::Water;
    if (sv == "Social" || sv == "social") return ResourceType::Social;
    return std::nullopt;
}

} // namespace genesis::world

