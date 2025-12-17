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

    // --- Social belief sharing (when agents socialize) ---
    float socialBeliefShareStrength{0.06f}; // 0 disables sharing
    float socialBeliefShareMinDeviation{0.10f}; // ignore near-prior beliefs
    std::uint32_t socialBeliefShareTopK{2}; // top-K interactions to share per partner
};

class LearningSystem {
public:
    explicit LearningSystem(LearningSystemConfig config = {});

    void update(entt::registry& registry, float deltaSeconds) const;

private:
    LearningSystemConfig m_config;
};

} // namespace genesis::agents
