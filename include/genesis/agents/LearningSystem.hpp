#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace genesis::agents {

struct LearningSystemConfig {
    // --- Experience (bufferMultiplier) ---
    float experienceMinMultiplier{1.0f};
    float experienceMaxMultiplier{3.0f};
    float experiencePreemptBump{0.18f};
    float experienceStockoutVitalBump{0.06f};

    // --- Beliefs (EMA) ---
    float beliefStockoutAlpha{0.22f};
    float beliefForgetPerSecond{0.06f};

    // --- Partner beliefs (EMA) ---
    float beliefPartnerReliabilityAlpha{0.18f};
    float beliefPartnerRejectAlphaFactor{1.00f};
    float beliefPartnerTimeoutAlphaFactor{0.35f};
    float beliefPartnerNoShowAlphaFactor{1.25f};
    float beliefPartnerForgetPerSecond{0.03f};
    float beliefPartnerMinAbsDeviationToKeep{0.01f};

    // --- Social belief sharing (when agents socialize) ---
    float socialBeliefShareStrength{0.06f}; // 0 disables sharing
    float socialBeliefShareMinDeviation{0.10f}; // ignore near-prior beliefs
    std::uint32_t socialBeliefShareTopK{2}; // top-K interactions to share per partner

    // --- Social relationship memory (agent-to-agent affinity) ---
    float relationshipForgetPerSecond{0.03f}; // decay toward neutral (0.0)
    float relationshipBondGain{0.08f}; // initiator gain on successful socialize
    float relationshipBondGainReciprocal{0.04f}; // partner gain on successful socialize
    float relationshipSnubPenalty{0.10f}; // initiator penalty on failed socialize
    float relationshipMinAbsToKeep{0.01f}; // prune small magnitudes
};

class LearningSystem {
public:
    explicit LearningSystem(LearningSystemConfig config = {});

    void update(entt::registry& registry, float deltaSeconds) const;

private:
    LearningSystemConfig m_config;
};

} // namespace genesis::agents
