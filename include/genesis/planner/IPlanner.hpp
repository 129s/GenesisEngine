#pragma once

#include <cstdint>

#include <entt/entt.hpp>

#include "genesis/world/WorldRegistry.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

namespace genesis::planner {

struct PlannerContext {
    entt::registry& registry;
    world::WorldRegistry& world;
    world::system::ResourceSystem& resourceSystem;
};

class IPlanner {
public:
    virtual ~IPlanner() = default;

    virtual void evaluate(std::uint64_t step, PlannerContext& context) = 0;
};

} // namespace genesis::planner

