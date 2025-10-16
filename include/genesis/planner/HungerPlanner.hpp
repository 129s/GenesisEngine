#pragma once

#include <memory>

#include "genesis/agents/NeedSatisfier.hpp"
#include "genesis/planner/IPlanner.hpp"

namespace genesis::planner {

class HungerPlanner : public IPlanner {
public:
    explicit HungerPlanner(genesis::agents::NeedSatisfierConfig config);

    void evaluate(std::uint64_t step, PlannerContext& context) override;

private:
    genesis::agents::NeedSatisfier m_satisfier;
};

} // namespace genesis::planner

