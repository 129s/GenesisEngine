#include "genesis/agents/MovementSystem.hpp"

#include <algorithm>
#include <queue>
#include <unordered_map>

namespace genesis::agents {

namespace {
constexpr float kEpsilon = 1e-4f;
}

MovementSystem::MovementSystem(genesis::world::WorldRegistry& world)
    : m_world(world) {
}

void MovementSystem::update(entt::registry& registry, float deltaSeconds) {
    if (m_world.empty()) {
        return;
    }

    auto view = registry.view<components::AgentLocation, components::MovementIntent>();
    for (auto entity : view) {
        auto& location = view.get<components::AgentLocation>(entity);
        auto& intent = view.get<components::MovementIntent>(entity);

        if (intent.target == genesis::world::InvalidLocation) {
            registry.remove<components::MovementIntent>(entity);
            registry.remove<components::MovementState>(entity);
            continue;
        }

        if (!m_world.findLocation(intent.target) || !m_world.findLocation(location.location)) {
            registry.remove<components::MovementIntent>(entity);
            registry.remove<components::MovementState>(entity);
            continue;
        }

        if (location.location == intent.target) {
            registry.remove<components::MovementIntent>(entity);
            registry.remove<components::MovementState>(entity);
            continue;
        }

        auto* state = registry.try_get<components::MovementState>(entity);
        if (!state) {
            state = &registry.emplace<components::MovementState>(entity);
        }

        if (state->destination != intent.target || state->path.empty()) {
            state->destination = intent.target;
            state->path = buildPath(location.location, intent.target);
            state->currentIndex = 0;
            state->distanceRemaining = 0.0f;
            state->accumulatedDistances.clear();
            state->traveledAlongEdge = 0.0f;
            state->blocked = false;

            if (state->path.empty()) {
                state->blocked = true;
            } else if (state->path.front() != location.location) {
                state->path.insert(state->path.begin(), location.location);
            }

            if (state->blocked || state->path.size() <= 1U) {
                if (!state->blocked) {
                    location.location = intent.target;
                }
                registry.remove<components::MovementIntent>(entity);
                registry.remove<components::MovementState>(entity);
                continue;
            }

            state->currentIndex = 1U;
            state->distanceRemaining = edgeCost(state->path[0], state->path[1]);
            state->accumulatedDistances.resize(state->path.size());
            state->accumulatedDistances[0] = 0.0f;
            float cumulative = 0.0f;
            for (std::size_t i = 1; i < state->path.size(); ++i) {
                cumulative += edgeCost(state->path[i - 1], state->path[i]);
                state->accumulatedDistances[i] = cumulative;
            }
            state->traveledAlongEdge = 0.0f;
        }

        if (state->blocked) {
            registry.remove<components::MovementIntent>(entity);
            registry.remove<components::MovementState>(entity);
            continue;
        }

        float speed = std::max(intent.speed, 0.0f);
        if (speed <= 0.0f) {
            continue;
        }

        float travel = speed * deltaSeconds;

        while (travel > kEpsilon && state->currentIndex < state->path.size()) {
            if (state->distanceRemaining <= kEpsilon) {
                location.location = state->path[state->currentIndex];
                ++state->currentIndex;

                if (state->currentIndex >= state->path.size()) {
                    registry.remove<components::MovementIntent>(entity);
                    registry.remove<components::MovementState>(entity);
                    break;
                }

                const auto from = location.location;
                const auto to = state->path[state->currentIndex];
                state->distanceRemaining = edgeCost(from, to);
                state->traveledAlongEdge = 0.0f;
                continue;
            }

            if (travel + kEpsilon >= state->distanceRemaining) {
                travel -= state->distanceRemaining;
                state->traveledAlongEdge += state->distanceRemaining;
                state->distanceRemaining = 0.0f;
                continue;
            }

            state->distanceRemaining -= travel;
            state->traveledAlongEdge += travel;
            travel = 0.0f;
        }

        if (state->currentIndex >= state->path.size()) {
            continue;
        }

        if (state->distanceRemaining <= kEpsilon) {
            location.location = state->path[state->currentIndex];
            ++state->currentIndex;

            if (state->currentIndex >= state->path.size()) {
                registry.remove<components::MovementIntent>(entity);
                registry.remove<components::MovementState>(entity);
            } else {
                const auto from = location.location;
                const auto to = state->path[state->currentIndex];
                state->distanceRemaining = edgeCost(from, to);
                state->traveledAlongEdge = 0.0f;
            }
        }
    }
}

std::vector<MovementSystem::LocationId> MovementSystem::buildPath(LocationId start, LocationId target) const {
    if (start == genesis::world::InvalidLocation || target == genesis::world::InvalidLocation) {
        return {};
    }

    if (!m_world.findLocation(start) || !m_world.findLocation(target)) {
        return {};
    }

    using Node = std::pair<float, LocationId>;
    auto cmp = [](const Node& lhs, const Node& rhs) { return lhs.first > rhs.first; };
    std::priority_queue<Node, std::vector<Node>, decltype(cmp)> frontier(cmp);

    std::unordered_map<LocationId, float, genesis::world::LocationIdHasher> distance;
    std::unordered_map<LocationId, LocationId, genesis::world::LocationIdHasher> previous;

    distance[start] = 0.0f;
    frontier.emplace(0.0f, start);

    while (!frontier.empty()) {
        const auto [cost, current] = frontier.top();
        frontier.pop();

        if (cost > distance[current] + kEpsilon) {
            continue;
        }

        if (current == target) {
            break;
        }

        for (const auto& edge : m_world.edgesFrom(current)) {
            const float stepCost = std::max(edge.cost, 0.0f);
            const float nextCost = cost + stepCost;

            auto [it, inserted] = distance.emplace(edge.to, nextCost);
            if (!inserted && nextCost + kEpsilon >= it->second) {
                continue;
            }

            it->second = nextCost;
            previous[edge.to] = current;
            frontier.emplace(nextCost, edge.to);
        }
    }

    if (!distance.contains(target)) {
        return {};
    }

    std::vector<LocationId> path;
    LocationId current = target;
    path.push_back(current);

    while (current != start) {
        auto it = previous.find(current);
        if (it == previous.end()) {
            return {};
        }
        current = it->second;
        path.push_back(current);
    }

    std::reverse(path.begin(), path.end());
    return path;
}

float MovementSystem::edgeCost(LocationId from, LocationId to) const {
    const auto& edges = m_world.edgesFrom(from);
    for (const auto& edge : edges) {
        if (edge.to == to) {
            return std::max(edge.cost, 0.0f);
        }
    }
    return 0.0f;
}

} // namespace genesis::agents
