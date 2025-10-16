#pragma once

#include <memory>
#include <vector>

#include <entt/entt.hpp>

#include "genesis/agents/NeedSatisfier.hpp"
#include "genesis/planner/IPlanner.hpp"

namespace genesis::planner {

struct HungerDecision {
    entt::entity agent{entt::null};
    world::LocationId target{world::InvalidLocation};
    float travelCost{0.0f};
    float score{0.0f};
};

struct LocationChoice {
    world::LocationId target{world::InvalidLocation};
    float travelCost{0.0f};
    float score{0.0f};
};

class HungerPlanner : public IPlanner {
public:
    HungerPlanner(genesis::agents::NeedSatisfierConfig config, LocationChoice (*locator)(PlannerContext&, entt::entity) = nullptr);

    void evaluate(std::uint64_t step, PlannerContext& context) override;

private:
    PlannerContext* m_currentContext{nullptr};
    std::vector<HungerDecision>* m_decisions{nullptr};
    LocationChoice (*m_locator)(PlannerContext&, entt::entity);
    genesis::agents::NeedSatisfier m_satisfier;
};

} // namespace genesis::planner
