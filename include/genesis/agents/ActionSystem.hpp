#pragma once

#include <cstdint>
#include <deque>

#include <entt/entt.hpp>

#include "genesis/agents/AgentComponents.hpp"
#include "genesis/agents/NeedSystem.hpp"
#include "genesis/agents/Needs.hpp"
#include "genesis/world/WorldTypes.hpp"

namespace genesis::world {
class WorldRegistry;

namespace system {
class ResourceSystem;
} // namespace system
} // namespace genesis::world

namespace genesis::agents {

enum class ActionType {
    MoveTo,
    ConsumeResource
};

struct ActionTask {
    ActionType type{ActionType::MoveTo};
    genesis::world::LocationId location{genesis::world::InvalidLocation};
    float speed{1.0f};
    genesis::world::ResourceType resource{genesis::world::ResourceType::Food};
    std::uint32_t amount{0};
    float reliefPerUnit{0.0f};
};

struct ActionQueue {
    std::deque<ActionTask> tasks;
};

class ActionExecutor {
public:
    ActionExecutor(genesis::world::WorldRegistry& world, genesis::world::system::ResourceSystem& resources);

    void requestMove(entt::entity entity, genesis::world::LocationId target, float speed, entt::registry& registry);
    void requestConsume(entt::entity entity, genesis::world::LocationId location, genesis::world::ResourceType type, std::uint32_t amount, float reliefPerUnit, entt::registry& registry);

    void update(entt::registry& registry, float deltaSeconds);

    [[nodiscard]] bool hasPendingActions(entt::entity entity, const entt::registry& registry) const;

private:
    void ensureQueue(entt::entity entity, entt::registry& registry);
    bool hasPendingConsume(const ActionQueue& queue, genesis::world::LocationId location, genesis::world::ResourceType type) const;
    void processMove(entt::entity entity, ActionQueue& queue, components::AgentLocation& location, entt::registry& registry);
    void processConsume(entt::entity entity, ActionQueue& queue, components::AgentLocation& location, entt::registry& registry);

    genesis::world::WorldRegistry& m_world;
    genesis::world::system::ResourceSystem& m_resources;
};

} // namespace genesis::agents
