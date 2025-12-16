#include "genesis/agents/LearningSystem.hpp"

#include <algorithm>
#include <cstddef>

#include "genesis/agents/Beliefs.hpp"
#include "genesis/agents/Experience.hpp"
#include "genesis/agents/Outcomes.hpp"
#include "genesis/agents/Personality.hpp"

namespace genesis::agents {

namespace {

[[nodiscard]] bool isVitalNeed(NeedType need) noexcept {
    switch (need) {
    case NeedType::Hunger:
    case NeedType::Thirst:
        return true;
    default:
        return false;
    }
}

void decayExperience(entt::registry& registry,
                     float deltaSeconds,
                     float minMultiplier,
                     float maxMultiplier) {
    if (!(deltaSeconds > 0.0f)) {
        return;
    }
    auto view = registry.view<components::AgentExperience>();
    for (auto entity : view) {
        auto& exp = view.get<components::AgentExperience>(entity);
        const float k = std::clamp(exp.forgetPerSecond * deltaSeconds, 0.0f, 1.0f);
        if (k <= 0.0f) {
            continue;
        }
        for (auto& m : exp.bufferMultiplier) {
            const float delta = m - 1.0f;
            m = 1.0f + delta * (1.0f - k);
            m = std::clamp(m, minMultiplier, maxMultiplier);
        }
    }
}

void bumpExperience(entt::entity entity,
                    NeedType need,
                    float bump,
                    float minMultiplier,
                    float maxMultiplier,
                    entt::registry& registry) {
    if (!(bump > 0.0f)) {
        return;
    }
    auto& exp = registry.get_or_emplace<components::AgentExperience>(entity);
    const auto idx = needIndex(need);
    if (idx >= exp.bufferMultiplier.size()) {
        return;
    }
    float m = std::clamp(exp.bufferMultiplier[idx], minMultiplier, maxMultiplier);
    m *= (1.0f + bump);
    exp.bufferMultiplier[idx] = std::clamp(m, minMultiplier, maxMultiplier);
}

void decayBeliefs(entt::registry& registry, float deltaSeconds, float forgetPerSecond) {
    if (!(deltaSeconds > 0.0f) || !(forgetPerSecond > 0.0f)) {
        return;
    }
    const float k = std::clamp(forgetPerSecond * deltaSeconds, 0.0f, 1.0f);
    if (k <= 0.0f) {
        return;
    }
    auto view = registry.view<components::AgentBeliefs>();
    for (auto entity : view) {
        auto& beliefs = view.get<components::AgentBeliefs>(entity);
        for (auto& [_, b] : beliefs.interactions) {
            b.stockoutRiskEma = std::clamp(
                b.stockoutRiskEma + (beliefs.priorStockoutRisk - b.stockoutRiskEma) * k,
                0.0f,
                1.0f);
        }
    }
}

[[nodiscard]] float learningScale(entt::entity entity, const entt::registry& registry) noexcept {
    if (const auto* p = registry.try_get<AgentPersonalityBig5>(entity)) {
        const float n = std::clamp(p->neuroticism, 0.0f, 1.0f);
        const float c = std::clamp(p->conscientiousness, 0.0f, 1.0f);
        return std::clamp(0.75f + 0.65f * n + 0.25f * c, 0.50f, 1.60f);
    }
    return 1.0f;
}

} // namespace

LearningSystem::LearningSystem(LearningSystemConfig config)
    : m_config(std::move(config)) {}

void LearningSystem::update(entt::registry& registry, float deltaSeconds) const {
    decayExperience(registry, deltaSeconds, m_config.experienceMinMultiplier, m_config.experienceMaxMultiplier);
    decayBeliefs(registry, deltaSeconds, m_config.beliefForgetPerSecond);

    auto view = registry.view<components::AgentOutcomeBuffer>();
    for (auto entity : view) {
        auto& outcomes = view.get<components::AgentOutcomeBuffer>(entity);
        if (outcomes.resourceAttempts.empty() && outcomes.criticalPreempts.empty()) {
            continue;
        }

        const float scale = learningScale(entity, registry);

        if (auto* beliefs = registry.try_get<components::AgentBeliefs>(entity)) {
            const float alpha = std::clamp(m_config.beliefStockoutAlpha * scale, 0.0f, 1.0f);
            for (const auto& o : outcomes.resourceAttempts) {
                if (o.interaction == 0) {
                    continue;
                }
                float obs = -1.0f;
                switch (o.failure) {
                case ResourceAttemptFailure::None:
                    if (o.obtainedUnits > 0U) {
                        obs = 0.0f;
                    }
                    break;
                case ResourceAttemptFailure::Stockout:
                    obs = o.recoveryPlanned ? 0.65f : 1.0f;
                    break;
                case ResourceAttemptFailure::PlanningFailed:
                    obs = 1.0f;
                    break;
                default:
                    break;
                }
                if (obs < 0.0f) {
                    continue;
                }

                auto& entry = beliefs->interactions[o.interaction];
                if (entry.stockoutRiskEma < 0.0f || entry.stockoutRiskEma > 1.0f) {
                    entry.stockoutRiskEma = beliefs->priorStockoutRisk;
                }
                entry.stockoutRiskEma = std::clamp(entry.stockoutRiskEma + (obs - entry.stockoutRiskEma) * alpha, 0.0f, 1.0f);
            }
        }

        for (const auto& preempt : outcomes.criticalPreempts) {
            bumpExperience(entity,
                           preempt.need,
                           m_config.experiencePreemptBump * scale,
                           m_config.experienceMinMultiplier,
                           m_config.experienceMaxMultiplier,
                           registry);
        }

        for (const auto& o : outcomes.resourceAttempts) {
            if (!isVitalNeed(o.need)) {
                continue;
            }
            if (o.failure == ResourceAttemptFailure::Stockout) {
                bumpExperience(entity,
                               o.need,
                               m_config.experienceStockoutVitalBump * scale,
                               m_config.experienceMinMultiplier,
                               m_config.experienceMaxMultiplier,
                               registry);
            } else if (o.failure == ResourceAttemptFailure::PlanningFailed) {
                bumpExperience(entity,
                               o.need,
                               m_config.experienceStockoutVitalBump * 0.5f * scale,
                               m_config.experienceMinMultiplier,
                               m_config.experienceMaxMultiplier,
                               registry);
            }
        }

        outcomes.clear();
    }
}

} // namespace genesis::agents

