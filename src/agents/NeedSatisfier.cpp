#include "genesis/agents/NeedSatisfier.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "genesis/agents/ActionSystem.hpp"
#include "genesis/agents/Affordances.hpp"
#include "genesis/agents/Commitments.hpp"
#include "genesis/agents/PlannerTargetEncoding.hpp"
#include "genesis/agents/Beliefs.hpp"
#include "genesis/agents/Experience.hpp"
#include "genesis/agents/Needs.hpp"
#include "genesis/agents/Movement2D.hpp"
#include "genesis/agents/Personality.hpp"
#include "genesis/agents/Planner.hpp"
#include "genesis/agents/Relations.hpp"
#include "genesis/world/MapPathfinding.hpp"

namespace genesis::agents {

namespace {

[[nodiscard]] std::uint32_t encodeAgentTarget(entt::entity entity) noexcept {
    return encodeAgentPlannerTargetEntityId(static_cast<std::uint32_t>(entt::to_integral(entity)));
}

[[nodiscard]] bool isAgentTarget(std::uint32_t target) noexcept {
    return isAgentPlannerTarget(target);
}

[[nodiscard]] entt::entity decodeAgentTarget(std::uint32_t target) noexcept {
    return static_cast<entt::entity>(decodeAgentPlannerTargetEntityId(target));
}

[[nodiscard]] std::uint64_t mix_u64(std::uint64_t x) noexcept {
    // splitmix64 finalizer
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

[[nodiscard]] float signed01_from_u64(std::uint64_t x) noexcept {
    const std::uint64_t v = mix_u64(x) >> 11; // top 53 bits
    const double u01 = static_cast<double>(v) * (1.0 / 9007199254740992.0); // 2^53
    return static_cast<float>(u01) * 2.0f - 1.0f;
}

NeedSample ensureSample(const NeedComponent& component,
                        NeedType type,
                        const NeedDescriptor& descriptor,
                        const NeedState& state) {
    const auto idx = needIndex(type);
    if (component.lastSamples[idx].has_value()) {
        return *component.lastSamples[idx];
    }

    NeedSample sample{};
    sample.type = type;
    sample.value = state.value;
    sample.satisfied = state.value <= descriptor.satisfiedThreshold;
    sample.critical = state.value >= descriptor.criticalThreshold;
    return sample;
}

} // namespace

NeedSatisfier::NeedSatisfier(NeedSatisfierConfig config)
    : m_config(std::move(config)) {
    if (m_config.hungerPrepareMargin < 0.0f) {
        m_config.hungerPrepareMargin = 0.0f;
    }
    if (m_config.thirstPrepareMargin < 0.0f) {
        m_config.thirstPrepareMargin = 0.0f;
    }
    if (m_config.socialPrepareMargin < 0.0f) {
        m_config.socialPrepareMargin = 0.0f;
    }
}

void NeedSatisfier::update(entt::registry& registry,
                           world::WorldDatabase& db,
                           world::system::ResourceSystem& resourceSystem,
                           std::uint64_t stepIndex,
                           ActionExecutor* actionExecutor) const {
    struct SpawnInfo {
        world::ResourceType type{world::ResourceType::Food};
        std::uint32_t current{0};
        std::uint32_t capacity{0};
    };

    std::unordered_map<world::InteractionId, SpawnInfo> spawnByInteraction;
    spawnByInteraction.reserve(64);
    resourceSystem.forEachSpawn(registry, [&](const auto& spawn, const auto& inventory) {
        SpawnInfo info{};
        info.type = spawn.type;
        info.current = inventory.current;
        info.capacity = inventory.capacity;
        spawnByInteraction[spawn.interaction] = info;
    });

    std::unordered_map<world::InteractionId, std::uint32_t> demandByInteraction;
    demandByInteraction.reserve(64);
    auto demandView = registry.view<components::PlannerDecision>();
    for (auto entity : demandView) {
        const auto target = demandView.get<components::PlannerDecision>(entity).target;
        if (target != 0) {
            ++demandByInteraction[target];
        }
    }

    auto view = registry.view<NeedComponent, components::AgentLocation2D>();

    const auto isWorkshopInteraction = [](const world::Interaction& inter) -> bool {
        if (!inter.meta) {
            return false;
        }
        const auto& meta = *inter.meta;
        return meta.contains("workshop") && meta.at("workshop").is_object();
    };

    for (auto entity : view) {
        auto& component = view.get<NeedComponent>(entity);
        const auto& location = view.get<components::AgentLocation2D>(entity);

        if (actionExecutor && actionExecutor->hasPendingActions(entity, registry)) {
            continue;
        }
        if (registry.any_of<components::MovementIntent2D>(entity)) {
            continue;
        }

        const AgentPersonalityBig5 personality = [&]() -> AgentPersonalityBig5 {
            if (const auto* p = registry.try_get<AgentPersonalityBig5>(entity)) {
                return *p;
            }
            return {};
        }();

        const float openness = std::clamp(personality.openness, 0.0f, 1.0f);
        const float conscientiousness = std::clamp(personality.conscientiousness, 0.0f, 1.0f);
        const float extraversion = std::clamp(personality.extraversion, 0.0f, 1.0f);
        const float agreeableness = std::clamp(personality.agreeableness, 0.0f, 1.0f);
        const float neuroticism = std::clamp(personality.neuroticism, 0.0f, 1.0f);

        const float planningHorizon = std::clamp(
            1.0f + 1.10f * conscientiousness + 0.85f * neuroticism - 0.60f * openness,
            0.40f,
            2.20f);

        const float wDist = std::clamp(1.0f + 0.90f * conscientiousness - 0.70f * openness, 0.35f, 2.50f);
        const float wScar = std::clamp(1.0f + 1.20f * neuroticism + 0.60f * conscientiousness - 0.40f * openness, 0.20f, 3.00f);
        const float wCrowd = std::clamp(1.0f + 1.10f * neuroticism - 0.90f * extraversion + 0.60f * agreeableness, 0.20f, 3.00f);
        const float wRisk = std::clamp(0.60f + 1.25f * neuroticism + 0.35f * conscientiousness - 0.25f * openness, 0.15f, 3.00f);

        const float crossMapMultiplier = std::clamp(1.10f - 0.75f * openness, 0.25f, 1.50f);

        const float switchMargin = std::max(0.0f, m_config.switchScoreMargin)
            * std::clamp(0.65f + 0.85f * conscientiousness + 0.25f * neuroticism, 0.25f, 2.25f);

        auto& commitment = registry.get_or_emplace<components::AgentCommitment>(entity);

        const auto quantize01u16 = [](float v) -> std::uint64_t {
            const float clamped = std::clamp(v, 0.0f, 1.0f);
            const float scaled = clamped * 65535.0f;
            const auto q = static_cast<std::uint64_t>(std::lround(static_cast<double>(scaled)));
            return (q > 65535ull) ? 65535ull : q;
        };

        const std::uint64_t packedBig5 =
            quantize01u16(openness)
            | (quantize01u16(conscientiousness) << 16U)
            | (quantize01u16(extraversion) << 32U)
            | (quantize01u16(agreeableness) << 48U);
        const std::uint64_t personalityHash =
            mix_u64(packedBig5 ^ (quantize01u16(neuroticism) * 0x9E3779B97F4A7C15ull));

        const std::uint64_t entitySeed = (static_cast<std::uint64_t>(entt::to_integral(entity)) << 1U)
            ^ (static_cast<std::uint64_t>(location.mapId) << 33U)
            ^ personalityHash
            ^ 0xA43B7D1C5E0F123Bull;

        const float noiseSigma = std::clamp(
            12.0f * (0.10f + 0.90f * openness) * (1.05f - 0.65f * conscientiousness) * (0.75f + 0.50f * neuroticism),
            0.0f,
            18.0f);

        const float hungerBuffer = [&]() -> float {
            if (const auto* exp = registry.try_get<components::AgentExperience>(entity)) {
                const auto idx = needIndex(NeedType::Hunger);
                return std::clamp(exp->bufferMultiplier[idx], 1.0f, 3.0f);
            }
            return 1.0f;
        }();
        const float thirstBuffer = [&]() -> float {
            if (const auto* exp = registry.try_get<components::AgentExperience>(entity)) {
                const auto idx = needIndex(NeedType::Thirst);
                return std::clamp(exp->bufferMultiplier[idx], 1.0f, 3.0f);
            }
            return 1.0f;
        }();
        const float socialBuffer = [&]() -> float {
            if (const auto* exp = registry.try_get<components::AgentExperience>(entity)) {
                const auto idx = needIndex(NeedType::Social);
                return std::clamp(exp->bufferMultiplier[idx], 1.0f, 3.0f);
            }
            return 1.0f;
        }();

        const auto scaledUnits = [&](NeedType need, std::uint32_t baseUnits) -> std::uint32_t {
            float buffer = 1.0f;
            switch (need) {
            case NeedType::Hunger:
                buffer = hungerBuffer;
                break;
            case NeedType::Thirst:
                buffer = thirstBuffer;
                break;
            case NeedType::Social:
                buffer = socialBuffer;
                break;
            default:
                buffer = 1.0f;
                break;
            }
            const float horizon = planningHorizon * buffer;
            const auto scaled = static_cast<std::uint32_t>(std::lround(static_cast<double>(baseUnits) * horizon));
            return std::max<std::uint32_t>(1U, scaled);
        };

        const auto scaledPrepare = [&](NeedType need, float baseMargin) -> float {
            float buffer = 1.0f;
            switch (need) {
            case NeedType::Hunger:
                buffer = hungerBuffer;
                break;
            case NeedType::Thirst:
                buffer = thirstBuffer;
                break;
            case NeedType::Social:
                buffer = socialBuffer;
                break;
            default:
                buffer = 1.0f;
                break;
            }
            const float horizon = planningHorizon * buffer;
            return std::max(0.0f, baseMargin) * horizon;
        };

        const auto costToInteraction = [&](const world::Interaction& inter) -> float {
            if (inter.mapId == location.mapId) {
                const auto coord = inter.worldCoord();
                const float dx = static_cast<float>(coord.first) - location.x;
                const float dy = static_cast<float>(coord.second) - location.y;
                const float dist2 = dx * dx + dy * dy;
                return dist2;
            }

            const auto mapPath = world::shortestMapPath(db, location.mapId, inter.mapId);
            if (!mapPath) {
                return std::numeric_limits<float>::infinity();
            }

            const float penalty = std::max(0.0f, m_config.crossMapPenalty) * crossMapMultiplier;
            return penalty + static_cast<float>(mapPath->totalCost);
        };

        const auto urgency01 = [](const NeedState& state, const NeedDescriptor& descriptor) -> float {
            const float denom = std::max(1.0f, descriptor.maxValue - descriptor.satisfiedThreshold);
            const float x = (state.value - descriptor.satisfiedThreshold) / denom;
            return std::clamp(x, 0.0f, 1.0f);
        };

        using Candidate = Affordance;

        const auto previousTarget = [&]() -> world::InteractionId {
            if (auto* prev = registry.try_get<components::PlannerDecision>(entity)) {
                return prev->target;
            }
            return 0;
        }();

        const auto demandPenalty = [&](world::InteractionId interaction, world::InteractionId selfTarget) -> float {
            auto it = demandByInteraction.find(interaction);
            if (it == demandByInteraction.end()) {
                return 0.0f;
            }
            auto demand = it->second;
            if (interaction != 0 && interaction == selfTarget && demand > 0U) {
                --demand;
            }
            return static_cast<float>(demand) * std::max(0.0f, m_config.demandPenaltyPerAgent);
        };

        const auto activation01_with_prepare =
            [&](NeedType need, const NeedState& state, const NeedDescriptor& descriptor, float prepareThreshold) -> float {
            const auto sample = ensureSample(component, need, descriptor, state);
            if (state.value < prepareThreshold && !sample.critical) {
                return 0.0f;
            }
            const float denom = std::max(1.0f, descriptor.maxValue - prepareThreshold);
            const float x = (state.value - prepareThreshold) / denom;
            return std::clamp(x, 0.0f, 1.0f);
        };

        const auto buildCandidate = [&](NeedType need,
                                        world::ResourceType resource,
                                        std::uint32_t unitsPerRequest,
                                        float reliefPerUnit,
                                        float activation,
                                        world::InteractionId interaction,
                                        const SpawnInfo& spawnInfo) -> std::optional<Candidate> {
            if (interaction == 0) {
                return std::nullopt;
            }
            if (spawnInfo.type != resource) {
                return std::nullopt;
            }
            const auto inter = db.findInteraction(interaction);
            if (!inter || inter->kind != world::InteractionKind::Resource) {
                return std::nullopt;
            }
            const bool isWorkshop = isWorkshopInteraction(*inter);
            if (spawnInfo.current == 0U && !isWorkshop) {
                return std::nullopt;
            }

            const float travelCost = costToInteraction(*inter);
            const float scarcity01 = (spawnInfo.capacity == 0U)
                ? 1.0f
                : (1.0f - static_cast<float>(spawnInfo.current) / static_cast<float>(spawnInfo.capacity));
            const float scarcityPenalty = scarcity01 * 50.0f;
            const float crowdPenalty = demandPenalty(interaction, previousTarget);
            const float stockoutRisk01 = [&]() -> float {
                if (const auto* beliefs = registry.try_get<components::AgentBeliefs>(entity)) {
                    return std::clamp(beliefs->stockoutRisk(interaction), 0.0f, 1.0f);
                }
                return 0.15f;
            }();
            const float riskPenalty = stockoutRisk01 * std::max(0.0f, m_config.stockoutRiskPenalty);
            float jitter = 0.0f;
            if (noiseSigma > 0.0f) {
                const std::uint64_t h = entitySeed
                    ^ (static_cast<std::uint64_t>(stepIndex) * 0x9E3779B97F4A7C15ull)
                    ^ (static_cast<std::uint64_t>(interaction) * 0xD1B54A32D192ED03ull)
                    ^ (static_cast<std::uint64_t>(needIndex(need)) * 0x94D049BB133111EBull);
                jitter = signed01_from_u64(h) * noiseSigma;
            }

            Candidate c{};
            c.kind = AffordanceKind::ConsumeFromInteraction;
            c.primaryNeed = need;
            c.resource = resource;
            c.interactionId = interaction;
            c.targetEntityId = 0;
            c.travelCost = travelCost;
            c.units = unitsPerRequest;
            c.reliefPerUnit = reliefPerUnit;
            c.score = std::clamp(activation, 0.0f, 1.0f) * 1000.0f
                - wDist * travelCost
                - wScar * scarcityPenalty
                - wCrowd * crowdPenalty
                - wRisk * riskPenalty
                + jitter;
            return c;
        };

        const auto buildSocialPartnerCandidate = [&](NeedType need,
                                                     std::uint32_t unitsPerRequest,
                                                     float reliefPerUnit,
                                                     float activation,
                                                     entt::entity partnerEntity,
                                                     float partnerUrgency01,
                                                     float partnerAvailability01) -> std::optional<Candidate> {
            if (partnerEntity == entt::null || !registry.valid(partnerEntity)) {
                return std::nullopt;
            }
            if (!registry.all_of<components::AgentLocation2D>(partnerEntity)) {
                return std::nullopt;
            }
            const auto& partnerLoc = registry.get<components::AgentLocation2D>(partnerEntity);
            if (partnerLoc.mapId != location.mapId) {
                return std::nullopt;
            }
            const float dx = partnerLoc.x - location.x;
            const float dy = partnerLoc.y - location.y;
            const float travelCost = dx * dx + dy * dy;

            const world::InteractionId interaction = encodeAgentTarget(partnerEntity);
            const float crowdPenalty = demandPenalty(interaction, previousTarget);
            float jitter = 0.0f;
            if (noiseSigma > 0.0f) {
                const std::uint64_t h = entitySeed
                    ^ (static_cast<std::uint64_t>(stepIndex) * 0x9E3779B97F4A7C15ull)
                    ^ (static_cast<std::uint64_t>(interaction) * 0xD1B54A32D192ED03ull)
                    ^ (static_cast<std::uint64_t>(needIndex(need)) * 0x94D049BB133111EBull);
                jitter = signed01_from_u64(h) * noiseSigma;
            }

            // Prefer partners who also want social contact (more "natural" interactions).
            const float partnerBonus = 160.0f * std::clamp(partnerUrgency01, 0.0f, 1.0f);

            float affinity = 0.0f;
            if (const auto* rel = registry.try_get<components::AgentRelations>(entity)) {
                const auto partnerKey = static_cast<std::uint32_t>(entt::to_integral(partnerEntity));
                if (const auto it = rel->affinityByPartner.find(partnerKey); it != rel->affinityByPartner.end()) {
                    affinity = std::clamp(it->second, -1.0f, 1.0f);
                }
            }

            const float affinityWeight =
                std::max(0.0f, m_config.socialPartnerAffinityBonus)
                * std::clamp(0.25f + 0.75f * agreeableness, 0.0f, 1.0f)
                * std::clamp(0.25f + 0.75f * (1.0f - openness), 0.0f, 1.0f);
            const float affinityBonus = affinityWeight * affinity;

            Candidate c{};
            const float availability01 = std::clamp(partnerAvailability01, 0.0f, 1.0f);
            const float reliability01 = [&]() -> float {
                const auto partnerKey = static_cast<std::uint32_t>(entt::to_integral(partnerEntity));
                if (const auto* beliefs = registry.try_get<components::AgentBeliefs>(entity)) {
                    return std::clamp(beliefs->meetReliability(partnerKey), 0.0f, 1.0f);
                }
                return 0.65f;
            }();
            const float predicted01 = availability01 * reliability01;
            const float predictedPenalty = (1.0f - predicted01)
                * 240.0f
                * std::clamp(0.35f + 0.65f * conscientiousness, 0.0f, 1.0f);

            c.kind = AffordanceKind::SocializeWithAgent;
            c.primaryNeed = need;
            c.resource = world::ResourceType::Social;
            c.interactionId = interaction;
            c.targetEntityId = static_cast<std::uint32_t>(entt::to_integral(partnerEntity));
            c.travelCost = travelCost;
            c.units = unitsPerRequest;
            c.reliefPerUnit = reliefPerUnit;
            c.score = std::clamp(activation, 0.0f, 1.0f) * 1000.0f
                + predicted01 * (partnerBonus + affinityBonus)
                - wDist * travelCost
                - wCrowd * crowdPenalty
                - predictedPenalty
                + jitter;
            return c;
        };

        std::vector<Candidate> affordances;
        affordances.reserve(48);

        const auto emitAffordance = [&](const std::optional<Candidate>& candidate) {
            if (!candidate.has_value()) {
                return;
            }
            affordances.push_back(*candidate);
        };

        auto considerNeed = [&](NeedType need,
                                world::ResourceType resource,
                                std::uint32_t unitsPerRequest,
                                float reliefPerUnit,
                                float prepareMargin,
                                const std::function<world::InteractionId(entt::entity)>& preferredLocator) {
            auto* state = component.needs.state(need);
            const auto* descriptor = component.needs.descriptor(need);
            if (!state || !descriptor) {
                return;
            }

            const float prepareThreshold = descriptor->satisfiedThreshold + prepareMargin;
            const float activation = activation01_with_prepare(need, *state, *descriptor, prepareThreshold);
            if (activation <= 0.0f) {
                return;
            }

            // 1) preferred locator（如果存在）
            if (preferredLocator) {
                const auto preferred = preferredLocator(entity);
                auto spawnIt = spawnByInteraction.find(preferred);
                if (spawnIt != spawnByInteraction.end()) {
                    emitAffordance(buildCandidate(need,
                                                  resource,
                                                  unitsPerRequest,
                                                  reliefPerUnit,
                                                  activation,
                                                  preferred,
                                                  spawnIt->second));
                }
            }

            // 2) 遍历资源刷点，选“综合最优”的目标
            resourceSystem.forEachSpawn(registry, [&](const auto& spawn, const auto& inventory) {
                if (spawn.type != resource) {
                    return;
                }
                if (inventory.current == 0U) {
                    const auto inter = db.findInteraction(spawn.interaction);
                    if (!inter || !isWorkshopInteraction(*inter)) {
                        return;
                    }
                }
                SpawnInfo info{};
                info.type = spawn.type;
                info.current = inventory.current;
                info.capacity = inventory.capacity;
                emitAffordance(buildCandidate(need,
                                              resource,
                                              unitsPerRequest,
                                              reliefPerUnit,
                                              activation,
                                              spawn.interaction,
                                              info));
            });

            // 3) Social 特例：和其他 agent 互动（不依赖 Social 资源生产链）
            if (need == NeedType::Social) {
                auto partnerView = registry.view<NeedComponent, components::AgentLocation2D>();
                for (auto partner : partnerView) {
                    if (partner == entity) {
                        continue;
                    }
                    const auto& partnerNeeds = partnerView.get<NeedComponent>(partner);
                    auto* pState = partnerNeeds.needs.state(NeedType::Social);
                    const auto* pDesc = partnerNeeds.needs.descriptor(NeedType::Social);
                    float partnerUrgency = 0.0f;
                    if (pState && pDesc) {
                        partnerUrgency = urgency01(*pState, *pDesc);
                    }

                    float partnerAvailability = 1.0f;
                    if (registry.any_of<components::MovementIntent2D>(partner)) {
                        partnerAvailability *= 0.35f;
                    }
                    if (actionExecutor && actionExecutor->hasPendingActions(partner, registry)) {
                        partnerAvailability *= 0.45f;
                    }
                    emitAffordance(buildSocialPartnerCandidate(need,
                                                               unitsPerRequest,
                                                               reliefPerUnit,
                                                               activation,
                                                               partner,
                                                               partnerUrgency,
                                                               partnerAvailability));
                }
            }
        };

        struct NeedSpec {
            NeedType need{NeedType::Hunger};
            world::ResourceType resource{world::ResourceType::Food};
            std::uint32_t unitsPerRequest{1};
            float reliefPerUnit{1.0f};
            float prepareMargin{0.0f};
            std::function<world::InteractionId(entt::entity)> preferredLocator{};
        };

        const NeedSpec specs[] = {
            NeedSpec{NeedType::Hunger,
                     world::ResourceType::Food,
                     scaledUnits(NeedType::Hunger, m_config.hungerUnitsPerRequest),
                     m_config.hungerReliefPerUnit,
                     scaledPrepare(NeedType::Hunger, m_config.hungerPrepareMargin),
                     m_config.hungerPreferredLocator},
            NeedSpec{NeedType::Thirst,
                     world::ResourceType::Water,
                     scaledUnits(NeedType::Thirst, m_config.thirstUnitsPerRequest),
                     m_config.thirstReliefPerUnit,
                     scaledPrepare(NeedType::Thirst, m_config.thirstPrepareMargin),
                     m_config.thirstPreferredLocator},
            NeedSpec{NeedType::Social,
                     world::ResourceType::Social,
                     scaledUnits(NeedType::Social, m_config.socialUnitsPerRequest),
                     m_config.socialReliefPerUnit,
                     scaledPrepare(NeedType::Social, m_config.socialPrepareMargin),
                     m_config.socialPreferredLocator},
        };
        for (const auto& spec : specs) {
            considerNeed(spec.need,
                         spec.resource,
                         spec.unitsPerRequest,
                         spec.reliefPerUnit,
                         spec.prepareMargin,
                         spec.preferredLocator);
        }

        std::optional<Candidate> best;
        for (const auto& a : affordances) {
            if (!best || a.score > best->score) {
                best = a;
            }
        }

        auto committedCandidate = [&]() -> std::optional<Candidate> {
            if (commitment.targetInteractionId == 0U) {
                return std::nullopt;
            }
            for (const auto& a : affordances) {
                if (a.interactionId != commitment.targetInteractionId) {
                    continue;
                }
                if (a.kind != commitment.kind) {
                    continue;
                }
                if (a.kind == AffordanceKind::SocializeWithAgent) {
                    if (a.targetEntityId != commitment.targetEntityId) {
                        continue;
                    }
                } else {
                    if (a.resource != commitment.resource) {
                        continue;
                    }
                }
                return a;
            }
            return std::nullopt;
        }();

        if (committedCandidate.has_value()) {
            const float marginFactor = (stepIndex < commitment.holdUntilStep)
                ? 1.0f
                : std::clamp(m_config.commitmentExpiredMarginFactor, 0.0f, 1.0f);
            const float breakMargin = switchMargin * marginFactor;

            if (!best) {
                best = committedCandidate;
            } else if (best->interactionId != committedCandidate->interactionId || best->kind != committedCandidate->kind) {
                if (best->score < committedCandidate->score + breakMargin) {
                    best = committedCandidate;
                }
            }
        }

        if (!best || best->interactionId == 0) {
            commitment.targetInteractionId = 0U;
            commitment.targetEntityId = 0U;
            if (previousTarget != 0) {
                auto it = demandByInteraction.find(previousTarget);
                if (it != demandByInteraction.end() && it->second > 0U) {
                    --it->second;
                    if (it->second == 0U) {
                        demandByInteraction.erase(it);
                    }
                }
            }
            if (registry.any_of<components::PlannerDecision>(entity)) {
                registry.remove<components::PlannerDecision>(entity);
            }
            continue;
        }

        {
            const float hold01 = std::clamp(0.25f + 0.75f * conscientiousness, 0.0f, 1.0f);
            const auto minHold = std::max<std::uint32_t>(1U, m_config.commitmentHoldMinSteps);
            const auto maxHold = std::max<std::uint32_t>(minHold, m_config.commitmentHoldMaxSteps);
            const float holdStepsF = static_cast<float>(minHold)
                + (static_cast<float>(maxHold - minHold) * hold01);
            std::uint64_t holdSteps = static_cast<std::uint64_t>(std::llround(static_cast<double>(holdStepsF)));
            if (best->kind == AffordanceKind::SocializeWithAgent) {
                holdSteps = static_cast<std::uint64_t>(std::llround(static_cast<double>(holdSteps) * std::max(0.0f, m_config.commitmentSocialHoldFactor)));
                holdSteps = std::max<std::uint64_t>(1ull, holdSteps);
            }

            commitment.targetInteractionId = best->interactionId;
            commitment.kind = best->kind;
            commitment.targetEntityId = best->targetEntityId;
            commitment.resource = best->resource;
            commitment.startedAtStep = stepIndex;
            commitment.holdUntilStep = stepIndex + holdSteps;
        }

        if (previousTarget != 0 && previousTarget != best->interactionId) {
            auto it = demandByInteraction.find(previousTarget);
            if (it != demandByInteraction.end() && it->second > 0U) {
                --it->second;
                if (it->second == 0U) {
                    demandByInteraction.erase(it);
                }
            }
        }
        if (best->interactionId != 0 && best->interactionId != previousTarget) {
            ++demandByInteraction[best->interactionId];
        }

        registry.emplace_or_replace<components::PlannerDecision>(entity,
                                                                components::PlannerDecision{best->interactionId, best->travelCost, best->score});

        if (actionExecutor) {
            if (best->kind == AffordanceKind::SocializeWithAgent && best->targetEntityId != 0U) {
                const auto ticks = std::max<std::uint32_t>(1U, m_config.socialInteractTicksPerUnit) * std::max<std::uint32_t>(1U, best->units);
                actionExecutor->requestSocialize(entity, best->targetEntityId, ticks, best->reliefPerUnit * static_cast<float>(best->units), registry);
            } else {
                actionExecutor->requestConsume(entity,
                                               best->interactionId,
                                               best->primaryNeed,
                                               best->resource,
                                               best->units,
                                               best->reliefPerUnit,
                                               registry);
            }
            continue;
        }

        const auto consumed = resourceSystem.consumeFromInteraction(registry, best->interactionId, best->resource, best->units);
        if (consumed == 0U) {
            continue;
        }

        if (auto* state = component.needs.state(best->primaryNeed); state) {
            if (const auto* descriptor = component.needs.descriptor(best->primaryNeed); descriptor) {
                const float relief = static_cast<float>(consumed) * best->reliefPerUnit;
                state->value = std::max(descriptor->minValue, state->value - relief);
                state->clamp(*descriptor);
                component.lastSamples[needIndex(best->primaryNeed)] = std::nullopt;
            }
        }

    }
}

} // namespace genesis::agents
