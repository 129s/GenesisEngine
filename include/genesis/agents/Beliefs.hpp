#pragma once

#include <cstdint>
#include <unordered_map>

namespace genesis::agents::components {

struct InteractionBelief {
    // EMA in [0,1]. 0 means "almost never stockout", 1 means "almost always stockout".
    float stockoutRiskEma{0.15f};
};

struct PartnerBelief {
    // EMA in [0,1]. 0 means "almost never reachable/available", 1 means "almost always reachable/available".
    float meetReliabilityEma{0.65f};
};

struct AgentBeliefs {
    // Prior used when no observations exist (and as decay target).
    float priorStockoutRisk{0.15f};
    float priorMeetReliability{0.65f};

    // Per-interaction beliefs (keyed by WorldDatabase InteractionId).
    std::unordered_map<std::uint32_t, InteractionBelief> interactions;

    // Per-partner beliefs (keyed by partner entityId).
    std::unordered_map<std::uint32_t, PartnerBelief> partners;

    [[nodiscard]] float stockoutRisk(std::uint32_t interaction) const noexcept {
        auto it = interactions.find(interaction);
        if (it == interactions.end()) {
            return priorStockoutRisk;
        }
        return it->second.stockoutRiskEma;
    }

    [[nodiscard]] float meetReliability(std::uint32_t partnerEntityId) const noexcept {
        auto it = partners.find(partnerEntityId);
        if (it == partners.end()) {
            return priorMeetReliability;
        }
        return it->second.meetReliabilityEma;
    }
};

} // namespace genesis::agents::components
