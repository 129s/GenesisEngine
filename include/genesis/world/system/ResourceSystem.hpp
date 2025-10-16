#pragma once

#include <cstdint>
#include <vector>

#include <entt/entt.hpp>

#include "genesis/world/WorldRegistry.hpp"

namespace genesis::world::system {

class ResourceSystem {
public:
    explicit ResourceSystem(WorldRegistry& registry);

    void initialize(entt::registry& registry);
    void tick(entt::registry& registry, std::uint64_t stepIndex);

private:
    WorldRegistry& m_world;
    std::vector<entt::entity> m_spawnEntities;
    bool m_initialized{false};
};

} // namespace genesis::world::system

