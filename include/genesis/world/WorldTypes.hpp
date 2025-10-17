#pragma once

#include <compare>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace genesis::world {

struct LocationId {
    std::uint32_t value{0};

    constexpr auto operator<=>(const LocationId&) const = default;
};

struct LocationIdHasher {
    std::size_t operator()(const LocationId& id) const noexcept {
        return std::hash<std::uint32_t>{}(id.value);
    }
};

constexpr LocationId InvalidLocation{0};

enum class LocationKind : std::uint8_t {
    Region,
    Building,
    Room,
    Point
};

struct LocationNode {
    LocationId id;
    LocationId parent;
    std::string name;
    LocationKind kind{LocationKind::Point};
    bool navigable{true};
    std::string terrain;
};

struct PathEdge {
    LocationId from;
    LocationId to;
    float cost{1.0f};
    bool bidirectional{true};
};

enum class ResourceType : std::uint8_t {
    Food,
    Drink,
    Social
};

struct ResourceSpawn {
    std::string name;
    ResourceType type{ResourceType::Food};
    LocationId location;
    std::uint32_t capacity{0};
    std::uint32_t ratePerStep{0};
};

struct LocationGraph {
    std::vector<LocationNode> nodes;
    std::vector<PathEdge> edges;
    std::vector<ResourceSpawn> spawns;
};

} // namespace genesis::world
