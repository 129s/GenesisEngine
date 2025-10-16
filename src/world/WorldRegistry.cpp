#include "genesis/world/WorldRegistry.hpp"

#include <algorithm>

namespace genesis::world {

void WorldRegistry::clear() {
    m_locations.clear();
    m_edges.clear();
    m_children.clear();
    m_resourceSpawns.clear();
}

void WorldRegistry::setGraph(LocationGraph graph) {
    clear();

    for (const auto& node : graph.nodes) {
        addLocation(node);
    }

    for (const auto& edge : graph.edges) {
        addEdge(edge);
    }

    for (const auto& spawn : graph.spawns) {
        addResourceSpawn(spawn);
    }
}

bool WorldRegistry::addLocation(const LocationNode& node) {
    if (node.id == InvalidLocation) {
        return false;
    }

    auto [it, inserted] = m_locations.emplace(node.id, node);
    if (!inserted) {
        it->second = node;
    }

    if (node.parent != InvalidLocation) {
        auto& bucket = m_children[node.parent];
        if (std::find(bucket.begin(), bucket.end(), node.id) == bucket.end()) {
            bucket.push_back(node.id);
        }
    }

    m_edges.try_emplace(node.id);
    return inserted;
}

bool WorldRegistry::addEdge(const PathEdge& edge) {
    if (!m_locations.contains(edge.from) || !m_locations.contains(edge.to)) {
        return false;
    }

    auto& fromBucket = m_edges[edge.from];
    fromBucket.push_back(edge);

    if (edge.bidirectional) {
        PathEdge reverse{edge.to, edge.from, edge.cost, true};
        auto& toBucket = m_edges[edge.to];
        toBucket.push_back(reverse);
    }

    return true;
}

void WorldRegistry::addResourceSpawn(const ResourceSpawn& spawn) {
    if (spawn.location == InvalidLocation) {
        return;
    }

    if (!m_locations.contains(spawn.location)) {
        return;
    }

    m_resourceSpawns.push_back(spawn);
}

const LocationNode* WorldRegistry::findLocation(LocationId id) const {
    if (auto it = m_locations.find(id); it != m_locations.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<LocationId> WorldRegistry::childrenOf(LocationId id) const {
    if (auto it = m_children.find(id); it != m_children.end()) {
        return it->second;
    }
    return {};
}

const std::vector<PathEdge>& WorldRegistry::edgesFrom(LocationId id) const {
    if (auto it = m_edges.find(id); it != m_edges.end()) {
        return it->second;
    }
    return s_emptyEdges;
}

std::vector<ResourceSpawn> WorldRegistry::spawnsAt(LocationId location) const {
    std::vector<ResourceSpawn> results;
    for (const auto& spawn : m_resourceSpawns) {
        if (spawn.location == location) {
            results.push_back(spawn);
        }
    }
    return results;
}

} // namespace genesis::world


