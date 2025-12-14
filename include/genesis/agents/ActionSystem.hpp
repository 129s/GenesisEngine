#pragma once

#include <cstdint>
#include <deque>

#include <entt/entt.hpp>

#include "genesis/agents/NeedSystem.hpp"
#include "genesis/agents/Needs.hpp"
#include "genesis/agents/Movement2D.hpp"
#include "genesis/world/WorldDatabase.hpp"
#include "genesis/world/WorldTypes.hpp"

namespace genesis::world {
namespace system { class ResourceSystem; }
} // namespace genesis::world

namespace genesis::agents {

enum class ActionType {
    MoveToInteraction,
    ConsumeResource
};

struct ActionTask {
    ActionType type{ActionType::MoveToInteraction};
    genesis::world::InteractionId interaction{0};
    float speed{1.0f};
    NeedType need{NeedType::Hunger};
    genesis::world::ResourceType resource{genesis::world::ResourceType::Food};
    std::uint32_t amount{0};
    float reliefPerUnit{0.0f};
};

struct ActionQueue {
    std::deque<ActionTask> tasks;
};

class ActionExecutor {
public:
    ActionExecutor(genesis::world::WorldDatabase& db, genesis::world::system::ResourceSystem& resources);

    void requestMoveToInteraction(entt::entity entity, genesis::world::InteractionId target, float speed, entt::registry& registry);
    void requestConsume(entt::entity entity,
                        genesis::world::InteractionId interaction,
                        NeedType need,
                        genesis::world::ResourceType type,
                        std::uint32_t amount,
                        float reliefPerUnit,
                        entt::registry& registry);

    void update(entt::registry& registry, float deltaSeconds);

    [[nodiscard]] bool hasPendingActions(entt::entity entity, const entt::registry& registry) const;

private:
    void ensureQueue(entt::entity entity, entt::registry& registry);
    bool hasPendingConsume(const ActionQueue& queue, genesis::world::InteractionId interaction, genesis::world::ResourceType type) const;
    void processMove(entt::entity entity, ActionQueue& queue, genesis::agents::components::AgentLocation2D& location, entt::registry& registry);
    void processConsume(entt::entity entity, ActionQueue& queue, genesis::agents::components::AgentLocation2D& location, entt::registry& registry);

    genesis::world::WorldDatabase& m_db;
    genesis::world::system::ResourceSystem& m_resources;
};

} // namespace genesis::agents
