#include "genesis/agents/LearningSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <utility>
#include <vector>

#include "genesis/agents/Beliefs.hpp"
#include "genesis/agents/Experience.hpp"
#include "genesis/agents/Outcomes.hpp"
#include "genesis/agents/Personality.hpp"
#include "genesis/agents/Relations.hpp"

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

void decayPartnerBeliefs(entt::registry& registry,
                         float deltaSeconds,
                         float forgetPerSecond,
                         float minAbsDeviationToKeep) {
    if (!(deltaSeconds > 0.0f) || !(forgetPerSecond > 0.0f)) {
        return;
    }
    const float k = std::clamp(forgetPerSecond * deltaSeconds, 0.0f, 1.0f);
    if (k <= 0.0f) {
        return;
    }
    const float keepAbs = std::max(0.0f, minAbsDeviationToKeep);
    auto view = registry.view<components::AgentBeliefs>();
    for (auto entity : view) {
        auto& beliefs = view.get<components::AgentBeliefs>(entity);
        for (auto it = beliefs.partners.begin(); it != beliefs.partners.end();) {
            auto& b = it->second;
            b.meetReliabilityEma = std::clamp(
                b.meetReliabilityEma + (beliefs.priorMeetReliability - b.meetReliabilityEma) * k,
                0.0f,
                1.0f);
            if (keepAbs > 0.0f && std::abs(b.meetReliabilityEma - beliefs.priorMeetReliability) < keepAbs) {
                it = beliefs.partners.erase(it);
                continue;
            }
            ++it;
        }
    }
}

void decayRelations(entt::registry& registry,
                    float deltaSeconds,
                    float forgetPerSecond,
                    float minAbsToKeep) {
    if (!(deltaSeconds > 0.0f) || !(forgetPerSecond > 0.0f)) {
        return;
    }
    const float k = std::clamp(forgetPerSecond * deltaSeconds, 0.0f, 1.0f);
    if (k <= 0.0f) {
        return;
    }
    const float keepAbs = std::max(0.0f, minAbsToKeep);
    auto view = registry.view<components::AgentRelations>();
    for (auto entity : view) {
        auto& rel = view.get<components::AgentRelations>(entity);
        for (auto it = rel.affinityByPartner.begin(); it != rel.affinityByPartner.end();) {
            it->second = std::clamp(it->second * (1.0f - k), -1.0f, 1.0f);
            if (keepAbs > 0.0f && std::abs(it->second) < keepAbs) {
                it = rel.affinityByPartner.erase(it);
                continue;
            }
            ++it;
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

[[nodiscard]] float socialShareWeight(const LearningSystemConfig& config, entt::entity receiver, const entt::registry& registry) noexcept {
    if (!(config.socialBeliefShareStrength > 0.0f)) {
        return 0.0f;
    }
    float agreeableness = 0.5f;
    if (const auto* p = registry.try_get<AgentPersonalityBig5>(receiver)) {
        agreeableness = std::clamp(p->agreeableness, 0.0f, 1.0f);
    }
    const float scale = learningScale(receiver, registry);
    const float w = config.socialBeliefShareStrength * scale * (0.25f + 0.75f * agreeableness);
    return std::clamp(w, 0.0f, 1.0f);
}

[[nodiscard]] float relationshipScale(entt::entity entity, const entt::registry& registry) noexcept {
    float agreeableness = 0.5f;
    float neuroticism = 0.5f;
    if (const auto* p = registry.try_get<AgentPersonalityBig5>(entity)) {
        agreeableness = std::clamp(p->agreeableness, 0.0f, 1.0f);
        neuroticism = std::clamp(p->neuroticism, 0.0f, 1.0f);
    }
    // Agreeable agents form bonds faster; neurotic agents punish snubs harder (handled in update).
    return std::clamp(0.70f + 0.60f * agreeableness, 0.35f, 1.35f);
}

} // namespace

LearningSystem::LearningSystem(LearningSystemConfig config)
    : m_config(std::move(config)) {}

void LearningSystem::update(entt::registry& registry, float deltaSeconds) const {
    decayExperience(registry, deltaSeconds, m_config.experienceMinMultiplier, m_config.experienceMaxMultiplier);
    decayBeliefs(registry, deltaSeconds, m_config.beliefForgetPerSecond);
    decayPartnerBeliefs(registry,
                        deltaSeconds,
                        m_config.beliefPartnerForgetPerSecond,
                        m_config.beliefPartnerMinAbsDeviationToKeep);
    decayRelations(registry, deltaSeconds, m_config.relationshipForgetPerSecond, m_config.relationshipMinAbsToKeep);

    auto view = registry.view<components::AgentOutcomeBuffer>();

    // --- Relationship memory update (agent-to-agent affinity) ---
    struct DirectedSocial {
        entt::entity src{entt::null};
        entt::entity dst{entt::null};
        bool success{true};

        [[nodiscard]] bool operator<(const DirectedSocial& other) const noexcept {
            const auto si = static_cast<std::uint32_t>(entt::to_integral(src));
            const auto di = static_cast<std::uint32_t>(entt::to_integral(dst));
            const auto sj = static_cast<std::uint32_t>(entt::to_integral(other.src));
            const auto dj = static_cast<std::uint32_t>(entt::to_integral(other.dst));
            if (si != sj) return si < sj;
            if (di != dj) return di < dj;
            return static_cast<int>(success) < static_cast<int>(other.success);
        }
    };

    std::vector<DirectedSocial> directedSocial;
    directedSocial.reserve(64);
    for (auto entity : view) {
        const auto& outcomes = view.get<components::AgentOutcomeBuffer>(entity);
        for (const auto& s : outcomes.socialInteractions) {
            if (s.partnerEntityId == 0U) {
                continue;
            }
            const auto partner = static_cast<entt::entity>(s.partnerEntityId);
            if (partner == entt::null || !registry.valid(partner) || partner == entity) {
                continue;
            }
            directedSocial.push_back(DirectedSocial{entity, partner, s.success});
        }
    }
    std::sort(directedSocial.begin(), directedSocial.end());

    if (!directedSocial.empty()
        && (m_config.relationshipBondGain != 0.0f
            || m_config.relationshipBondGainReciprocal != 0.0f
            || m_config.relationshipSnubPenalty != 0.0f)) {
        auto bumpAffinity = [&](entt::entity who, entt::entity toward, float delta) {
            if (!(delta != 0.0f)) {
                return;
            }
            auto& rel = registry.get_or_emplace<components::AgentRelations>(who);
            const auto key = static_cast<std::uint32_t>(entt::to_integral(toward));
            float& v = rel.affinityByPartner[key];
            v = std::clamp(v + delta, -1.0f, 1.0f);
        };

        for (const auto& e : directedSocial) {
            const float srcScale = relationshipScale(e.src, registry);
            if (e.success) {
                bumpAffinity(e.src, e.dst, m_config.relationshipBondGain * srcScale);
                const float dstScale = relationshipScale(e.dst, registry);
                bumpAffinity(e.dst, e.src, m_config.relationshipBondGainReciprocal * dstScale);
            } else {
                float neuroticism = 0.5f;
                if (const auto* p = registry.try_get<AgentPersonalityBig5>(e.src)) {
                    neuroticism = std::clamp(p->neuroticism, 0.0f, 1.0f);
                }
                const float penaltyScale = std::clamp(0.70f + 0.80f * neuroticism, 0.35f, 1.60f);
                bumpAffinity(e.src, e.dst, -m_config.relationshipSnubPenalty * srcScale * penaltyScale);
            }
        }
    }

    // --- Partner beliefs update (meet reliability) ---
    if (!directedSocial.empty() && m_config.beliefPartnerReliabilityAlpha > 0.0f) {
        for (const auto& e : directedSocial) {
            auto* beliefs = registry.try_get<components::AgentBeliefs>(e.src);
            if (!beliefs) {
                continue;
            }
            const float scale = learningScale(e.src, registry);
            const float alpha = std::clamp(m_config.beliefPartnerReliabilityAlpha * scale, 0.0f, 1.0f);
            if (alpha <= 0.0f) {
                continue;
            }

            const auto partnerKey = static_cast<std::uint32_t>(entt::to_integral(e.dst));
            if (partnerKey == 0U) {
                continue;
            }
            auto& entry = beliefs->partners[partnerKey];
            if (entry.meetReliabilityEma < 0.0f || entry.meetReliabilityEma > 1.0f) {
                entry.meetReliabilityEma = beliefs->priorMeetReliability;
            }
            const float obs = e.success ? 1.0f : 0.0f;
            entry.meetReliabilityEma = std::clamp(entry.meetReliabilityEma + (obs - entry.meetReliabilityEma) * alpha, 0.0f, 1.0f);
        }
    }

    // --- Social belief sharing: build a stable set of unique interaction pairs for this tick. ---
    struct SocialPair {
        entt::entity a{entt::null};
        entt::entity b{entt::null};

        [[nodiscard]] bool operator<(const SocialPair& other) const noexcept {
            const auto ai = static_cast<std::uint32_t>(entt::to_integral(a));
            const auto bi = static_cast<std::uint32_t>(entt::to_integral(b));
            const auto aj = static_cast<std::uint32_t>(entt::to_integral(other.a));
            const auto bj = static_cast<std::uint32_t>(entt::to_integral(other.b));
            if (ai != aj) return ai < aj;
            return bi < bj;
        }

        [[nodiscard]] bool operator==(const SocialPair& other) const noexcept {
            return a == other.a && b == other.b;
        }
    };

    std::vector<SocialPair> socialPairs;
    if (m_config.socialBeliefShareStrength > 0.0f && m_config.socialBeliefShareTopK > 0U) {
        for (auto entity : view) {
            const auto& outcomes = view.get<components::AgentOutcomeBuffer>(entity);
            for (const auto& s : outcomes.socialInteractions) {
                if (s.partnerEntityId == 0U) {
                    continue;
                }
                const auto partner = static_cast<entt::entity>(s.partnerEntityId);
                if (partner == entt::null || !registry.valid(partner)) {
                    continue;
                }
                if (partner == entity) {
                    continue;
                }
                SocialPair pair{entity, partner};
                const auto ai = static_cast<std::uint32_t>(entt::to_integral(pair.a));
                const auto bi = static_cast<std::uint32_t>(entt::to_integral(pair.b));
                if (bi < ai) {
                    std::swap(pair.a, pair.b);
                }
                socialPairs.push_back(pair);
            }
        }
        std::sort(socialPairs.begin(), socialPairs.end());
        socialPairs.erase(std::unique(socialPairs.begin(), socialPairs.end()), socialPairs.end());
    }

    // Snapshot beliefs to avoid order dependence in the main loop.
    struct BeliefSnapshot {
        float priorStockoutRisk{0.15f};
        std::unordered_map<std::uint32_t, float> stockoutRiskEmaByInteraction;
    };

    std::unordered_map<std::uint32_t, BeliefSnapshot> beliefSnapshots;
    beliefSnapshots.reserve(socialPairs.size() * 2U);
    auto snapshotBeliefs = [&](entt::entity entity) {
        const auto key = static_cast<std::uint32_t>(entt::to_integral(entity));
        if (beliefSnapshots.find(key) != beliefSnapshots.end()) {
            return;
        }
        if (const auto* beliefs = registry.try_get<components::AgentBeliefs>(entity)) {
            BeliefSnapshot snap;
            snap.priorStockoutRisk = beliefs->priorStockoutRisk;
            snap.stockoutRiskEmaByInteraction.reserve(beliefs->interactions.size());
            for (const auto& [interactionId, b] : beliefs->interactions) {
                snap.stockoutRiskEmaByInteraction.emplace(interactionId, std::clamp(b.stockoutRiskEma, 0.0f, 1.0f));
            }
            beliefSnapshots.emplace(key, std::move(snap));
        }
    };

    for (const auto& pair : socialPairs) {
        snapshotBeliefs(pair.a);
        snapshotBeliefs(pair.b);
    }

    struct BlendAccum {
        float weightSum{0.0f};
        float weightedValueSum{0.0f};
    };

    std::unordered_map<std::uint32_t, std::unordered_map<std::uint32_t, BlendAccum>> socialBlendByEntity;

    auto topInteractionsToShare = [&](const BeliefSnapshot& src) {
        struct Entry {
            std::uint32_t interaction{0};
            float risk{0.0f};
            float deviation{0.0f};
        };
        std::vector<Entry> items;
        items.reserve(src.stockoutRiskEmaByInteraction.size());
        for (const auto& [interaction, risk] : src.stockoutRiskEmaByInteraction) {
            const float dev = std::abs(risk - src.priorStockoutRisk);
            if (dev < std::max(0.0f, m_config.socialBeliefShareMinDeviation)) {
                continue;
            }
            items.push_back(Entry{interaction, risk, dev});
        }
        std::sort(items.begin(), items.end(), [](const Entry& a, const Entry& b) {
            if (a.deviation != b.deviation) return a.deviation > b.deviation;
            if (a.risk != b.risk) return a.risk > b.risk;
            return a.interaction < b.interaction;
        });
        if (items.size() > m_config.socialBeliefShareTopK) {
            items.resize(m_config.socialBeliefShareTopK);
        }
        return items;
    };

    auto addBlend = [&](entt::entity dst, std::uint32_t interaction, float w, float srcRisk) {
        if (!(w > 0.0f)) {
            return;
        }
        const auto key = static_cast<std::uint32_t>(entt::to_integral(dst));
        auto& m = socialBlendByEntity[key];
        auto& acc = m[interaction];
        acc.weightSum += w;
        acc.weightedValueSum += w * std::clamp(srcRisk, 0.0f, 1.0f);
    };

    for (const auto& pair : socialPairs) {
        const auto aKey = static_cast<std::uint32_t>(entt::to_integral(pair.a));
        const auto bKey = static_cast<std::uint32_t>(entt::to_integral(pair.b));
        const auto aIt = beliefSnapshots.find(aKey);
        const auto bIt = beliefSnapshots.find(bKey);
        if (aIt == beliefSnapshots.end() || bIt == beliefSnapshots.end()) {
            continue;
        }

        const float wab = socialShareWeight(m_config, pair.a, registry);
        const float wba = socialShareWeight(m_config, pair.b, registry);
        if (wab <= 0.0f && wba <= 0.0f) {
            continue;
        }

        const auto topA = topInteractionsToShare(aIt->second);
        const auto topB = topInteractionsToShare(bIt->second);

        for (const auto& e : topB) {
            addBlend(pair.a, e.interaction, wab, e.risk);
        }
        for (const auto& e : topA) {
            addBlend(pair.b, e.interaction, wba, e.risk);
        }
    }

    for (const auto& [entityKey, blends] : socialBlendByEntity) {
        const auto entity = static_cast<entt::entity>(entityKey);
        auto* beliefs = registry.try_get<components::AgentBeliefs>(entity);
        if (!beliefs) {
            continue;
        }
        const auto snapIt = beliefSnapshots.find(entityKey);
        if (snapIt == beliefSnapshots.end()) {
            continue;
        }
        const auto& snap = snapIt->second;

        for (const auto& [interaction, acc] : blends) {
            if (!(acc.weightSum > 0.0f)) {
                continue;
            }
            float base = snap.priorStockoutRisk;
            if (const auto it = snap.stockoutRiskEmaByInteraction.find(interaction); it != snap.stockoutRiskEmaByInteraction.end()) {
                base = it->second;
            }
            const float srcMean = acc.weightedValueSum / acc.weightSum;
            const float w = std::clamp(acc.weightSum, 0.0f, 1.0f);
            const float blended = std::clamp(base * (1.0f - w) + srcMean * w, 0.0f, 1.0f);
            auto& entry = beliefs->interactions[interaction];
            if (entry.stockoutRiskEma < 0.0f || entry.stockoutRiskEma > 1.0f) {
                entry.stockoutRiskEma = beliefs->priorStockoutRisk;
            }
            entry.stockoutRiskEma = blended;
        }
    }

    for (auto entity : view) {
        auto& outcomes = view.get<components::AgentOutcomeBuffer>(entity);
        if (outcomes.resourceAttempts.empty() && outcomes.criticalPreempts.empty()) {
            if (!outcomes.socialInteractions.empty()) {
                outcomes.clear();
            }
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
