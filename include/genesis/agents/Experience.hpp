#pragma once

#include <array>
#include <cstddef>

#include "genesis/agents/Needs.hpp"

namespace genesis::agents::components {

struct AgentExperience {
    // Multiplicative safety buffer per need. 1.0 means neutral.
    std::array<float, static_cast<std::size_t>(genesis::agents::NeedType::Count)> bufferMultiplier{};

    // Linear decay rate toward 1.0 per simulated second.
    float forgetPerSecond{0.15f};

    AgentExperience() {
        bufferMultiplier.fill(1.0f);
    }
};

} // namespace genesis::agents::components

