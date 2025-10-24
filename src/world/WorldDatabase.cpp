#include "genesis/world/WorldDatabase.hpp"

#include <algorithm>

namespace genesis::world {

void InMemoryWorldDatabase::clear() {
    maps_.clear();
    edges_.clear();
    scenes_.clear();
    interactions_.clear();
    portals_.clear();
}

void InMemoryWorldDatabase::addMap(Map m) { maps_.push_back(std::move(m)); }
void InMemoryWorldDatabase::addMapEdge(MapEdge e) { edges_.push_back(std::move(e)); }
void InMemoryWorldDatabase::addScene(Scene s) { scenes_.push_back(std::move(s)); }
void InMemoryWorldDatabase::addInteraction(Interaction i) { interactions_.push_back(std::move(i)); }
void InMemoryWorldDatabase::addPortal(Portal p) { portals_.push_back(std::move(p)); }

std::vector<Scene> InMemoryWorldDatabase::scenes(MapId mapId) const {
    std::vector<Scene> out;
    out.reserve(scenes_.size());
    for (const auto& s : scenes_) {
        if (s.mapId == mapId) out.push_back(s);
    }
    return out;
}

std::vector<Interaction> InMemoryWorldDatabase::interactions(MapId mapId) const {
    std::vector<Interaction> out;
    out.reserve(interactions_.size());
    for (const auto& i : interactions_) {
        if (i.mapId == mapId) out.push_back(i);
    }
    return out;
}

std::vector<Portal> InMemoryWorldDatabase::portals(MapId mapId) const {
    std::vector<Portal> out;
    out.reserve(portals_.size());
    for (const auto& p : portals_) {
        // 通过 interactionId 无法直接确定 mapId，这里简单按目标 Map 过滤，常见查询也会用 targetMapId
        if (p.targetMapId == mapId) out.push_back(p);
    }
    return out;
}

std::vector<MapEdge> InMemoryWorldDatabase::mapEdgesFrom(MapId from) const {
    std::vector<MapEdge> out;
    out.reserve(edges_.size());
    for (const auto& e : edges_) {
        if (e.from == from) out.push_back(e);
        if (e.bidirectional && e.to == from) {
            MapEdge rev{ e.to, e.from, e.bidirectional };
            out.push_back(rev);
        }
    }
    return out;
}

std::optional<Interaction> InMemoryWorldDatabase::findInteraction(InteractionId id) const {
    auto it = std::find_if(interactions_.begin(), interactions_.end(), [&](const Interaction& i){ return i.id == id; });
    if (it != interactions_.end()) return *it;
    return std::nullopt;
}

} // namespace genesis::world

