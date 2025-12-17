#pragma once

#include <cstdint>
#include <unordered_map>

namespace genesis::agents::components {

// Agent-to-agent relationship memory (positive = affinity, negative = aversion).
// Neutral is 0.0. Values are expected to be kept within [-1, 1] by update systems.
struct AgentRelations {
    std::unordered_map<std::uint32_t, float> affinityByPartner;
};

} // namespace genesis::agents::components

