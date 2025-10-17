#pragma once

#include <compare>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <optional>
#include <utility>

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
    // Map View global grid coordinate (tile index). When not provided, tools may synthesize,
    // but runtime components assume presence after schema migration.
    std::optional<std::pair<int, int>> coord_global;
};

struct PathEdge {
    LocationId from;
    LocationId to;
    float cost{1.0f};
    bool bidirectional{true};
    // Optional Map geometry for edge drawing (grid polyline) and Scene portal anchors.
    std::vector<std::pair<int, int>> polyline; // grid coords in Map space (optional)
    std::optional<std::pair<int, int>> anchor_at_from; // Scene local grid coord at 'from'
    std::optional<std::pair<int, int>> anchor_at_to;   // Scene local grid coord at 'to'
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
    // Scene local grid coordinate of the interaction/resource point
    std::optional<std::pair<int, int>> local_coord;
};

struct LocationGraph {
    std::vector<LocationNode> nodes;
    std::vector<PathEdge> edges;
    std::vector<ResourceSpawn> spawns;
    std::uint32_t schemaVersion{0};
};

} // namespace genesis::world
