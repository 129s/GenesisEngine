#pragma once

#include <vector>

#include <entt/entt.hpp>

#include "genesis/agents/AgentComponents.hpp"
#include "genesis/world/WorldRegistry.hpp"

namespace genesis::agents {

class MovementSystem {
public:
    explicit MovementSystem(genesis::world::WorldRegistry& world);

    void update(entt::registry& registry, float deltaSeconds);

private:
    using LocationId = genesis::world::LocationId;

    std::vector<LocationId> buildPath(LocationId start, LocationId target) const;
    float edgeCost(LocationId from, LocationId to) const;

    genesis::world::WorldRegistry& m_world;
};

} // namespace genesis::agents

