#include "genesis/agents/ActionSystem.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include "genesis/agents/Needs.hpp"
#include "genesis/world/MapPathfinding.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

namespace genesis::agents {

namespace {
constexpr float kMinSpeed = 0.1f;
} // namespace

ActionExecutor::ActionExecutor(genesis::world::WorldDatabase& db, genesis::world::system::ResourceSystem& resources)
    : m_db(db)
    , m_resources(resources) {}

void ActionExecutor::requestMoveToInteraction(entt::entity entity,
                                              genesis::world::InteractionId target,
                                              float speed,
                                              entt::registry& registry) {
    ensureQueue(entity, registry);
    auto& queue = registry.get<ActionQueue>(entity);

    if (!queue.tasks.empty()) {
        return;
    }

    ActionTask move{};
    move.type = ActionType::MoveToInteraction;
    move.interaction = target;
    move.speed = std::max(speed, kMinSpeed);
    queue.tasks.push_back(move);
}

void ActionExecutor::requestConsume(entt::entity entity,
                                    genesis::world::InteractionId interaction,
                                    NeedType need,
                                    genesis::world::ResourceType type,
                                    std::uint32_t amount,
                                    float reliefPerUnit,
                                    entt::registry& registry) {
    ensureQueue(entity, registry);
    auto& queue = registry.get<ActionQueue>(entity);

    if (hasPendingConsume(queue, interaction, type)) {
        return;
    }

    if (queue.tasks.empty()) {
        const auto* agentLocation = registry.try_get<components::AgentLocation2D>(entity);
        bool atTarget = false;
        genesis::world::MapId targetMap{0};
        float tx = 0.0f, ty = 0.0f;
        if (auto it = m_db.findInteraction(interaction)) {
            targetMap = it->mapId;
            const auto coord = it->worldCoord();
            tx = static_cast<float>(coord.first);
            ty = static_cast<float>(coord.second);
            if (agentLocation) {
                atTarget = (agentLocation->mapId == targetMap && agentLocation->x == tx && agentLocation->y == ty);
            }
        }
        if (!atTarget) {
            ActionTask move{};
            move.type = ActionType::MoveToInteraction;
            move.interaction = interaction;
            move.speed = 1.0f;
            queue.tasks.push_back(move);
        }
    }

    ActionTask consume{};
    consume.type = ActionType::ConsumeResource;
    consume.interaction = interaction;
    consume.need = need;
    consume.resource = type;
    consume.amount = amount;
    consume.reliefPerUnit = reliefPerUnit;
    queue.tasks.push_back(consume);
}

void ActionExecutor::update(entt::registry& registry, float /*deltaSeconds*/) {
    auto view = registry.view<ActionQueue, components::AgentLocation2D>();

    for (auto entity : view) {
        auto& queue = view.get<ActionQueue>(entity);
        auto& location = view.get<components::AgentLocation2D>(entity);

        bool advanced = true;
        while (advanced && !queue.tasks.empty()) {
            auto& task = queue.tasks.front();
            advanced = false;

            switch (task.type) {
            case ActionType::MoveToInteraction:
                processMove(entity, queue, location, registry);
                advanced = false;
                break;
            case ActionType::ConsumeResource:
                processConsume(entity, queue, location, registry);
                advanced = true;
                break;
            }
        }

        if (queue.tasks.empty()) {
            registry.remove<ActionQueue>(entity);
            if (registry.any_of<components::MovementIntent2D>(entity)) {
                registry.remove<components::MovementIntent2D>(entity);
            }
        }
    }
}

bool ActionExecutor::hasPendingActions(entt::entity entity, const entt::registry& registry) const {
    if (!registry.all_of<ActionQueue>(entity)) {
        return false;
    }
    const auto& queue = registry.get<ActionQueue>(entity);
    return !queue.tasks.empty();
}

void ActionExecutor::ensureQueue(entt::entity entity, entt::registry& registry) {
    if (!registry.all_of<ActionQueue>(entity)) {
        registry.emplace<ActionQueue>(entity);
    }
}

bool ActionExecutor::hasPendingConsume(const ActionQueue& queue,
                                       genesis::world::InteractionId interaction,
                                       genesis::world::ResourceType type) const {
    return std::any_of(queue.tasks.begin(), queue.tasks.end(), [&](const ActionTask& task) {
        return task.type == ActionType::ConsumeResource && task.interaction == interaction && task.resource == type;
    });
}

void ActionExecutor::processMove(entt::entity entity,
                                 ActionQueue& queue,
                                 components::AgentLocation2D& location,
                                 entt::registry& registry) {
    if (queue.tasks.empty()) {
        return;
    }

    auto& task = queue.tasks.front();
    auto it = m_db.findInteraction(task.interaction);
    if (!it) {
        queue.tasks.pop_front();
        return;
    }

    const auto targetMap = it->mapId;
    const auto coord = it->worldCoord();
    const float tx = static_cast<float>(coord.first);
    const float ty = static_cast<float>(coord.second);

    if (location.mapId == targetMap && location.x == tx && location.y == ty) {
        queue.tasks.pop_front();
        if (registry.any_of<components::MovementIntent2D>(entity)) {
            registry.remove<components::MovementIntent2D>(entity);
        }
        return;
    }

    if (location.mapId != targetMap) {
        const auto path = genesis::world::shortestMapPath(m_db, location.mapId, targetMap);
        if (!path || path->maps.size() < 2) {
            queue.tasks.pop_front();
            if (registry.any_of<components::MovementIntent2D>(entity)) {
                registry.remove<components::MovementIntent2D>(entity);
            }
            return;
        }

        const auto nextMap = path->maps[1];
        std::optional<genesis::world::Portal> bestPortal;
        float bestCost = std::numeric_limits<float>::infinity();

        for (const auto& portal : m_db.portals(location.mapId)) {
            if (portal.targetMapId != nextMap) {
                continue;
            }
            auto portalInter = m_db.findInteraction(portal.interactionId);
            if (!portalInter) {
                continue;
            }
            const auto portalCoord = portalInter->worldCoord();
            const float px = static_cast<float>(portalCoord.first);
            const float py = static_cast<float>(portalCoord.second);
            const float dx = px - location.x;
            const float dy = py - location.y;
            const float dist2 = dx * dx + dy * dy;
            const float cost = dist2 + static_cast<float>(std::max(0.0, portal.teleportCost));
            if (cost < bestCost) {
                bestCost = cost;
                bestPortal = portal;
            }
        }

        if (!bestPortal) {
            queue.tasks.pop_front();
            if (registry.any_of<components::MovementIntent2D>(entity)) {
                registry.remove<components::MovementIntent2D>(entity);
            }
            return;
        }

        auto portalInter = m_db.findInteraction(bestPortal->interactionId);
        if (!portalInter) {
            queue.tasks.pop_front();
            if (registry.any_of<components::MovementIntent2D>(entity)) {
                registry.remove<components::MovementIntent2D>(entity);
            }
            return;
        }

        const auto portalCoord = portalInter->worldCoord();
        const float px = static_cast<float>(portalCoord.first);
        const float py = static_cast<float>(portalCoord.second);

        auto& intent = registry.get_or_emplace<components::MovementIntent2D>(entity);
        if (location.x == px && location.y == py) {
            const auto targetCoord = bestPortal->targetCoord.value_or(std::make_pair(0, 0));
            intent.targetMapId = bestPortal->targetMapId;
            intent.targetX = static_cast<float>(targetCoord.first);
            intent.targetY = static_cast<float>(targetCoord.second);
            intent.speed = 0.0f;
        } else {
            intent.targetMapId = location.mapId;
            intent.targetX = px;
            intent.targetY = py;
            intent.speed = std::max(task.speed, kMinSpeed);
        }
        return;
    }

    auto& intent = registry.get_or_emplace<components::MovementIntent2D>(entity);
    intent.targetMapId = targetMap;
    intent.targetX = tx;
    intent.targetY = ty;
    intent.speed = std::max(task.speed, kMinSpeed);
}

void ActionExecutor::processConsume(entt::entity entity,
                                    ActionQueue& queue,
                                    components::AgentLocation2D& location,
                                    entt::registry& registry) {
    if (queue.tasks.empty()) {
        return;
    }

    auto& task = queue.tasks.front();
    auto it = m_db.findInteraction(task.interaction);
    if (!it) {
        queue.tasks.pop_front();
        return;
    }

    const auto targetMap = it->mapId;
    const auto coord = it->worldCoord();
    const float tx = static_cast<float>(coord.first);
    const float ty = static_cast<float>(coord.second);

    if (!(location.mapId == targetMap && location.x == tx && location.y == ty)) {
        ActionTask move{};
        move.type = ActionType::MoveToInteraction;
        move.interaction = task.interaction;
        move.speed = 1.0f;
        queue.tasks.emplace(queue.tasks.begin(), move);
        return;
    }

    auto* needs = registry.try_get<NeedComponent>(entity);
    if (!needs) {
        queue.tasks.pop_front();
        return;
    }

    auto* state = needs->needs.state(task.need);
    const auto* descriptor = needs->needs.descriptor(task.need);
    if (!state || !descriptor) {
        queue.tasks.pop_front();
        return;
    }

    const auto consumed = m_resources.consume(registry, task.resource, task.amount, task.interaction);
    if (consumed > 0U) {
        const float relief = static_cast<float>(consumed) * task.reliefPerUnit;
        state->value = std::max(descriptor->minValue, state->value - relief);
        state->clamp(*descriptor);
        needs->lastSamples[needIndex(task.need)] = std::nullopt;
    }

    queue.tasks.pop_front();
}

} // namespace genesis::agents
