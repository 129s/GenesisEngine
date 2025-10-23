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
            state->segmentLength = state->distanceRemaining;
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
                state->segmentLength = state->distanceRemaining;
                state->traveledAlongEdge = 0.0f;
                continue;
            }

            if (travel + kEpsilon >= state->distanceRemaining) {
                const float consumed = state->distanceRemaining;
                travel -= consumed;
                state->traveledAlongEdge += consumed;
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
                state->segmentLength = state->distanceRemaining;
                state->traveledAlongEdge = 0.0f;
            }
        }
    }
}

std::vector<MovementSystem::LocationId> MovementSystem::buildPath(LocationId start, LocationId target) const {
    // 直线语义：同一 Map 内直接从起点到终点，仅返回两点路径。
    if (start == genesis::world::InvalidLocation || target == genesis::world::InvalidLocation) {
        return {};
    }
    if (!m_world.findLocation(start) || !m_world.findLocation(target)) {
        return {};
    }

    if (start == target) {
        return {start};
    }
    return {start, target};
}

float MovementSystem::edgeCost(LocationId from, LocationId to) const {
    // 距离采用节点全局整格坐标的欧氏距离；若缺失则回退为 1.0。
    const auto* nfrom = m_world.findLocation(from);
    const auto* nto = m_world.findLocation(to);
    if (!nfrom || !nto) {
        return 1.0f;
    }
    if (nfrom->coord_global && nto->coord_global) {
        const float dx = static_cast<float>(nto->coord_global->first - nfrom->coord_global->first);
        const float dy = static_cast<float>(nto->coord_global->second - nfrom->coord_global->second);
        const float d = std::sqrt(dx * dx + dy * dy);
        return d > kEpsilon ? d : 1.0f;
    }
    return 1.0f;
}

} // namespace genesis::agents
