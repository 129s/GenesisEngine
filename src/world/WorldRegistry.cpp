#include "genesis/world/WorldRegistry.hpp"

#include <algorithm>
#include <unordered_set>

namespace genesis::world {

void WorldRegistry::clear() {
    m_locations.clear();
    m_edges.clear();
    m_children.clear();
    m_resourceSpawns.clear();
    m_tilemaps.clear();
    m_schemaVersion = 0;
}

void WorldRegistry::setGraph(LocationGraph graph) {
    clear();

    m_schemaVersion = graph.schemaVersion;

    for (const auto& node : graph.nodes) {
        addLocation(node);
    }

    for (const auto& edge : graph.edges) {
        addEdge(edge);
    }

    for (const auto& spawn : graph.spawns) {
        addResourceSpawn(spawn);
    }

    // store tilemaps meta
    m_tilemaps = std::move(graph.tilemaps);
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
        // Carry geometry and anchors, swapping endpoints
        reverse.polyline = edge.polyline; // optional: kept same for drawing; direction-agnostic in Map
        reverse.anchor_at_from = edge.anchor_at_to;
        reverse.anchor_at_to = edge.anchor_at_from;
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

std::vector<LocationNode> WorldRegistry::locations() const {
    std::vector<LocationNode> nodes;
    nodes.reserve(m_locations.size());
    for (const auto& [id, node] : m_locations) {
        nodes.push_back(node);
    }
    return nodes;
}

LocationGraph WorldRegistry::exportGraph() const {
    LocationGraph graph{};
    graph.schemaVersion = m_schemaVersion;

    graph.nodes.reserve(m_locations.size());
    for (const auto& [id, node] : m_locations) {
        graph.nodes.push_back(node);
    }
    std::sort(graph.nodes.begin(), graph.nodes.end(), [](const LocationNode& a, const LocationNode& b) {
        return a.id.value < b.id.value;
    });

    graph.spawns = m_resourceSpawns;
    std::sort(graph.spawns.begin(), graph.spawns.end(), [](const ResourceSpawn& a, const ResourceSpawn& b) {
        if (a.location.value != b.location.value) {
            return a.location.value < b.location.value;
        }
        return a.name < b.name;
    });

    graph.tilemaps = m_tilemaps;
    std::sort(graph.tilemaps.begin(), graph.tilemaps.end(), [](const TilemapMeta& a, const TilemapMeta& b) {
        return a.node.value < b.node.value;
    });

    std::unordered_set<std::uint64_t> emittedPairs;
    emittedPairs.reserve(m_edges.size());
    for (const auto& [fromId, edges] : m_edges) {
        for (const auto& edge : edges) {
            if (edge.bidirectional) {
                const std::uint32_t minId = std::min(edge.from.value, edge.to.value);
                const std::uint32_t maxId = std::max(edge.from.value, edge.to.value);
                const std::uint64_t key = (static_cast<std::uint64_t>(minId) << 32) | static_cast<std::uint64_t>(maxId);
                if (!emittedPairs.insert(key).second) {
                    continue;
                }
            }
            graph.edges.push_back(edge);
        }
    }

    std::sort(graph.edges.begin(), graph.edges.end(), [](const PathEdge& a, const PathEdge& b) {
        if (a.from.value != b.from.value) {
            return a.from.value < b.from.value;
        }
        return a.to.value < b.to.value;
    });

    return graph;
}

} // namespace genesis::world


