#include "genesis/world/MapPathfinding.hpp"

#include <algorithm>
#include <limits>
#include <queue>
#include <unordered_map>

namespace genesis::world {

namespace {

struct QueueNode {
    MapId map{0};
    double cost{0.0};
    bool operator>(const QueueNode& other) const { return cost > other.cost; }
};

[[nodiscard]] bool isEdgeEnabled(const MapEdge& edge) {
    if (!edge.rules) {
        return true;
    }
    const auto& rules = *edge.rules;
    if (rules.is_object()) {
        auto it = rules.find("disabled");
        if (it != rules.end() && it->is_boolean() && it->get<bool>()) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] double effectiveEdgeCost(const MapEdge& edge) {
    double cost = std::max(0.0, edge.cost);
    if (!edge.rules) {
        return cost;
    }

    const auto& rules = *edge.rules;
    if (!rules.is_object()) {
        return cost;
    }

    if (auto it = rules.find("costMultiplier"); it != rules.end() && it->is_number()) {
        const double mul = it->get<double>();
        if (mul >= 0.0) {
            cost *= mul;
        }
    }
    if (auto it = rules.find("extraCost"); it != rules.end() && it->is_number()) {
        const double extra = it->get<double>();
        if (extra >= 0.0) {
            cost += extra;
        }
    }
    return cost;
}

} // namespace

std::optional<MapPath> shortestMapPath(const WorldDatabase& db, MapId start, MapId target) {
    if (start == 0 || target == 0) {
        return std::nullopt;
    }

    if (start == target) {
        MapPath path{};
        path.maps = {start};
        path.totalCost = 0.0;
        return path;
    }

    std::unordered_map<MapId, double> best;
    std::unordered_map<MapId, MapId> prev;

    std::priority_queue<QueueNode, std::vector<QueueNode>, std::greater<QueueNode>> queue;
    best[start] = 0.0;
    queue.push(QueueNode{start, 0.0});

    while (!queue.empty()) {
        const auto current = queue.top();
        queue.pop();

        auto bestIt = best.find(current.map);
        if (bestIt == best.end() || current.cost > bestIt->second + 1e-12) {
            continue;
        }

        if (current.map == target) {
            break;
        }

        for (const auto& edge : db.mapEdgesFrom(current.map)) {
            if (edge.to == 0) {
                continue;
            }
            if (!isEdgeEnabled(edge)) {
                continue;
            }
            const double nextCost = current.cost + effectiveEdgeCost(edge);
            auto it = best.find(edge.to);
            if (it == best.end() || nextCost < it->second) {
                best[edge.to] = nextCost;
                prev[edge.to] = current.map;
                queue.push(QueueNode{edge.to, nextCost});
            }
        }
    }

    auto targetIt = best.find(target);
    if (targetIt == best.end()) {
        return std::nullopt;
    }

    std::vector<MapId> maps;
    for (MapId cur = target; cur != 0; ) {
        maps.push_back(cur);
        if (cur == start) {
            break;
        }
        auto it = prev.find(cur);
        if (it == prev.end()) {
            return std::nullopt;
        }
        cur = it->second;
    }
    std::reverse(maps.begin(), maps.end());

    MapPath path{};
    path.maps = std::move(maps);
    path.totalCost = targetIt->second;
    return path;
}

} // namespace genesis::world

