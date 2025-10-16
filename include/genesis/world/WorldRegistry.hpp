#pragma once

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "genesis/world/WorldTypes.hpp"

namespace genesis::world {

class WorldRegistry {
public:
    WorldRegistry() = default;

    void clear();

    void setGraph(LocationGraph graph);

    bool addLocation(const LocationNode& node);
    bool addEdge(const PathEdge& edge);
    void addResourceSpawn(const ResourceSpawn& spawn);

    [[nodiscard]] const LocationNode* findLocation(LocationId id) const;
    [[nodiscard]] std::vector<LocationId> childrenOf(LocationId id) const;
    [[nodiscard]] const std::vector<PathEdge>& edgesFrom(LocationId id) const;
    [[nodiscard]] const std::vector<ResourceSpawn>& resourceSpawns() const noexcept { return m_resourceSpawns; }

    [[nodiscard]] bool empty() const noexcept { return m_locations.empty(); }
    [[nodiscard]] std::size_t locationCount() const noexcept { return m_locations.size(); }
    [[nodiscard]] std::size_t resourceSpawnCount() const noexcept { return m_resourceSpawns.size(); }

    [[nodiscard]] std::vector<ResourceSpawn> spawnsAt(LocationId location) const;
    [[nodiscard]] const std::vector<ResourceSpawn>& allSpawns() const noexcept { return m_resourceSpawns; }

private:
    using LocationMap = std::unordered_map<LocationId, LocationNode, LocationIdHasher>;
    using EdgeMap = std::unordered_map<LocationId, std::vector<PathEdge>, LocationIdHasher>;
    using ChildMap = std::unordered_map<LocationId, std::vector<LocationId>, LocationIdHasher>;

    LocationMap m_locations;
    EdgeMap m_edges;
    ChildMap m_children;
    std::vector<ResourceSpawn> m_resourceSpawns;

    inline static const std::vector<PathEdge> s_emptyEdges{};
};

} // namespace genesis::world
