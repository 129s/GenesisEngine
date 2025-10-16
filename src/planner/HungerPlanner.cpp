#include "genesis/planner/HungerPlanner.hpp"

#include <entt/entt.hpp>

#include "genesis/agents/NeedSystem.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

namespace genesis::planner {

HungerPlanner::HungerPlanner(genesis::agents::NeedSatisfierConfig config)
    : m_satisfier(std::move(config)) {
}

void HungerPlanner::evaluate(std::uint64_t /*step*/, PlannerContext& context) {
    m_satisfier.update(context.registry, context.resourceSystem);
}

} // namespace genesis::planner

