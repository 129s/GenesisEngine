#include "genesis/agents/ActionSystem.hpp"

#include <algorithm>
#include <optional>

#include "genesis/agents/Needs.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

namespace genesis::agents {

namespace {
constexpr float kMinSpeed = 0.1f;
}

ActionExecutor::ActionExecutor(genesis::world::WorldRegistry& world, genesis::world::system::ResourceSystem& resources)
    : m_world(world)
    , m_resources(resources) {
}

void ActionExecutor::requestMove(entt::entity entity, genesis::world::LocationId target, float speed, entt::registry& registry) {
    ensureQueue(entity, registry);
    auto& queue = registry.get<ActionQueue>(entity);

    if (!queue.tasks.empty()) {
        return;
    }

    ActionTask move{};
    move.type = ActionType::MoveTo;
    move.location = target;
    move.speed = std::max(speed, kMinSpeed);
    queue.tasks.push_back(move);
}

void ActionExecutor::requestConsume(entt::entity entity, genesis::world::LocationId location, genesis::world::ResourceType type, std::uint32_t amount, float reliefPerUnit, entt::registry& registry) {
    ensureQueue(entity, registry);
    auto& queue = registry.get<ActionQueue>(entity);

    if (hasPendingConsume(queue, location, type)) {
        return;
    }

    if (queue.tasks.empty()) {
        const auto* agentLocation = registry.try_get<components::AgentLocation>(entity);
        if (!agentLocation || agentLocation->location != location) {
            ActionTask move{};
            move.type = ActionType::MoveTo;
            move.location = location;
            move.speed = 1.0f;
            queue.tasks.push_back(move);
        }
    }

    ActionTask consume{};
    consume.type = ActionType::ConsumeResource;
    consume.location = location;
    consume.resource = type;
    consume.amount = amount;
    consume.reliefPerUnit = reliefPerUnit;
    queue.tasks.push_back(consume);
}

void ActionExecutor::update(entt::registry& registry, float /*deltaSeconds*/) {
    auto view = registry.view<ActionQueue, components::AgentLocation>();

    for (auto entity : view) {
        auto& queue = view.get<ActionQueue>(entity);
        auto& location = view.get<components::AgentLocation>(entity);

        bool advanced = true;
        while (advanced && !queue.tasks.empty()) {
            auto& task = queue.tasks.front();
            advanced = false;

            switch (task.type) {
            case ActionType::MoveTo:
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
            if (registry.any_of<components::MovementIntent>(entity)) {
                registry.remove<components::MovementIntent>(entity);
            }
            if (registry.any_of<components::MovementState>(entity)) {
                registry.remove<components::MovementState>(entity);
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

bool ActionExecutor::hasPendingConsume(const ActionQueue& queue, genesis::world::LocationId location, genesis::world::ResourceType type) const {
    return std::any_of(queue.tasks.begin(), queue.tasks.end(), [&](const ActionTask& task) {
        return task.type == ActionType::ConsumeResource && task.location == location && task.resource == type;
    });
}

void ActionExecutor::processMove(entt::entity entity, ActionQueue& queue, components::AgentLocation& location, entt::registry& registry) {
    if (queue.tasks.empty()) {
        return;
    }

    auto& task = queue.tasks.front();
    if (location.location == task.location) {
        queue.tasks.pop_front();
        if (registry.any_of<components::MovementIntent>(entity)) {
            registry.remove<components::MovementIntent>(entity);
        }
        if (registry.any_of<components::MovementState>(entity)) {
            registry.remove<components::MovementState>(entity);
        }
        return;
    }

    auto& intent = registry.get_or_emplace<components::MovementIntent>(entity);
    intent.target = task.location;
    intent.speed = std::max(task.speed, kMinSpeed);
}

void ActionExecutor::processConsume(entt::entity entity, ActionQueue& queue, components::AgentLocation& location, entt::registry& registry) {
    if (queue.tasks.empty()) {
        return;
    }

    auto& task = queue.tasks.front();
    if (location.location != task.location) {
        ActionTask move{};
        move.type = ActionType::MoveTo;
        move.location = task.location;
        move.speed = 1.0f;
        queue.tasks.emplace(queue.tasks.begin(), move);
        return;
    }

    auto* needs = registry.try_get<NeedComponent>(entity);
    if (!needs) {
        queue.tasks.pop_front();
        return;
    }

    auto* hungerState = needs->needs.state(NeedType::Hunger);
    const auto* hungerDescriptor = needs->needs.descriptor(NeedType::Hunger);
    if (!hungerState || !hungerDescriptor) {
        queue.tasks.pop_front();
        return;
    }

    const auto consumed = m_resources.consume(registry, task.resource, task.amount, task.location);
    if (consumed > 0U) {
        const float relief = static_cast<float>(consumed) * task.reliefPerUnit;
        hungerState->value = std::max(hungerDescriptor->minValue, hungerState->value - relief);
        hungerState->clamp(*hungerDescriptor);
        needs->lastSamples[needIndex(NeedType::Hunger)] = std::nullopt;
    }

    queue.tasks.pop_front();
}

} // namespace genesis::agents

