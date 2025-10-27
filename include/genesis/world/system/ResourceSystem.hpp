#pragma once

#include <cstdint>
#include <vector>

#include <entt/entt.hpp>

#include "genesis/messaging/EventBus.hpp"
#include "genesis/world/WorldDatabase.hpp"
#include "genesis/world/components/ResourceInventory.hpp"
#include "genesis/world/components/ResourceSpawn.hpp"

namespace genesis::world::system {

class ResourceSystem {
public:
    ResourceSystem(WorldDatabase& database, genesis::messaging::EventBus& eventBus);

    void initialize(entt::registry& registry);
    void reset(entt::registry& registry);
    void tick(entt::registry& registry, std::uint64_t stepIndex);

    std::uint32_t consume(entt::registry& registry, genesis::world::ResourceType type, std::uint32_t amount, genesis::world::InteractionId preferred);

private:
    WorldDatabase& m_db;
    genesis::messaging::EventBus& m_eventBus;
    std::vector<entt::entity> m_spawnEntities;
    bool m_initialized{false};
};

} // namespace genesis::world::system

