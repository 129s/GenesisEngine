#pragma once

#include <memory>

#include "genesis/agents/NeedSatisfier.hpp"
#include "genesis/planner/IPlanner.hpp"

namespace genesis::planner {

class HungerPlanner : public IPlanner {
public:
    HungerPlanner(genesis::agents::NeedSatisfierConfig config, world::LocationId (*locator)(PlannerContext&, entt::entity) = nullptr);

    void evaluate(std::uint64_t step, PlannerContext& context) override;

private:
    PlannerContext* m_currentContext{nullptr};
    world::LocationId (*m_locator)(PlannerContext&, entt::entity);
    genesis::agents::NeedSatisfier m_satisfier;
};

} // namespace genesis::planner



