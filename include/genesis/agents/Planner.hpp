#pragma once

#include <cstdint>

#include <entt/entt.hpp>

#include "genesis/world/WorldDatabase.hpp"

namespace genesis::agents::components {

// 每 tick 生成的“当前决策”，用于可观测性（Telemetry）。
struct PlannerDecision {
    genesis::world::InteractionId target{0};
    float travelCost{0.0f};
    float score{0.0f};
};

} // namespace genesis::agents::components

