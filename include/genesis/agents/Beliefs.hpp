#pragma once

#include <cstdint>
#include <unordered_map>

namespace genesis::agents::components {

struct InteractionBelief {
    // EMA in [0,1]. 0 means "almost never stockout", 1 means "almost always stockout".
    float stockoutRiskEma{0.15f};
};

struct AgentBeliefs {
    // Prior used when no observations exist (and as decay target).
    float priorStockoutRisk{0.15f};

    // Per-interaction beliefs (keyed by WorldDatabase InteractionId).
    std::unordered_map<std::uint32_t, InteractionBelief> interactions;

    [[nodiscard]] float stockoutRisk(std::uint32_t interaction) const noexcept {
        auto it = interactions.find(interaction);
        if (it == interactions.end()) {
            return priorStockoutRisk;
        }
        return it->second.stockoutRiskEma;
    }
};

} // namespace genesis::agents::components
