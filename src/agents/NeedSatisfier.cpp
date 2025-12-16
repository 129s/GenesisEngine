#include "genesis/agents/NeedSatisfier.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>

#include "genesis/agents/ActionSystem.hpp"
#include "genesis/agents/Needs.hpp"
#include "genesis/agents/Movement2D.hpp"
#include "genesis/agents/Personality.hpp"
#include "genesis/agents/Planner.hpp"
#include "genesis/world/MapPathfinding.hpp"

namespace genesis::agents {

namespace {

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

        const float crossMapMultiplier = std::clamp(1.10f - 0.75f * openness, 0.25f, 1.50f);

        const float switchMargin = std::max(0.0f, m_config.switchScoreMargin)
            * std::clamp(0.65f + 0.85f * conscientiousness + 0.25f * neuroticism, 0.25f, 2.25f);

        const std::uint64_t entitySeed = (static_cast<std::uint64_t>(entt::to_integral(entity)) << 1U)
            ^ (static_cast<std::uint64_t>(location.mapId) << 33U)
            ^ 0xA43B7D1C5E0F123Bull;

        const float noiseSigma = std::clamp(
            12.0f * (0.10f + 0.90f * openness) * (1.05f - 0.65f * conscientiousness) * (0.75f + 0.50f * neuroticism),
            0.0f,
            18.0f);

        const auto scaledUnits = [&](std::uint32_t baseUnits) -> std::uint32_t {
            const auto scaled = static_cast<std::uint32_t>(std::lround(static_cast<double>(baseUnits) * planningHorizon));
            return std::max<std::uint32_t>(1U, scaled);
        };

        const auto scaledPrepare = [&](float baseMargin) -> float {
            return std::max(0.0f, baseMargin) * planningHorizon;
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

        struct Candidate {
            NeedType need{NeedType::Hunger};
            world::ResourceType resource{world::ResourceType::Food};
            world::InteractionId interaction{0};
            float travelCost{0.0f};
            float score{0.0f};
            std::uint32_t units{0};
            float reliefPerUnit{0.0f};
        };

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

        const auto buildCandidate = [&](NeedType need,
                                        world::ResourceType resource,
                                        std::uint32_t unitsPerRequest,
                                        float reliefPerUnit,
                                        float urgency,
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
            float jitter = 0.0f;
            if (noiseSigma > 0.0f) {
                const std::uint64_t h = entitySeed
                    ^ (static_cast<std::uint64_t>(stepIndex) * 0x9E3779B97F4A7C15ull)
                    ^ (static_cast<std::uint64_t>(interaction) * 0xD1B54A32D192ED03ull)
                    ^ (static_cast<std::uint64_t>(needIndex(need)) * 0x94D049BB133111EBull);
                jitter = signed01_from_u64(h) * noiseSigma;
            }

            Candidate c{};
            c.need = need;
            c.resource = resource;
            c.interaction = interaction;
            c.travelCost = travelCost;
            c.units = unitsPerRequest;
            c.reliefPerUnit = reliefPerUnit;
            c.score = urgency * 1000.0f - wDist * travelCost - wScar * scarcityPenalty - wCrowd * crowdPenalty + jitter;
            return c;
        };

        auto considerNeed = [&](NeedType need,
                                world::ResourceType resource,
                                std::uint32_t unitsPerRequest,
                                float reliefPerUnit,
                                float prepareMargin,
                                const std::function<world::InteractionId(entt::entity)>& preferredLocator,
                                std::optional<Candidate>& best) {
            auto* state = component.needs.state(need);
            const auto* descriptor = component.needs.descriptor(need);
            if (!state || !descriptor) {
                return;
            }

            const auto sample = ensureSample(component, need, *descriptor, *state);
            const float prepareThreshold = descriptor->satisfiedThreshold + prepareMargin;
            if (state->value < prepareThreshold && !sample.critical) {
                return;
            }

            const float urgency = urgency01(*state, *descriptor);
            if (urgency <= 0.0f) {
                return;
            }

            // 1) preferred locator（如果存在）
            if (preferredLocator) {
                const auto preferred = preferredLocator(entity);
                auto spawnIt = spawnByInteraction.find(preferred);
                if (spawnIt != spawnByInteraction.end()) {
                    if (auto candidate = buildCandidate(need,
                                                        resource,
                                                        unitsPerRequest,
                                                        reliefPerUnit,
                                                        urgency,
                                                        preferred,
                                                        spawnIt->second)) {
                        if (!best || candidate->score > best->score) {
                            best = std::move(candidate);
                        }
                    }
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
                if (auto candidate = buildCandidate(need,
                                                    resource,
                                                    unitsPerRequest,
                                                    reliefPerUnit,
                                                    urgency,
                                                    spawn.interaction,
                                                    info)) {
                    if (!best || candidate->score > best->score) {
                        best = std::move(candidate);
                    }
                }
            });
        };

        std::optional<Candidate> best;
        considerNeed(NeedType::Hunger,
                     world::ResourceType::Food,
                     scaledUnits(m_config.hungerUnitsPerRequest),
                     m_config.hungerReliefPerUnit,
                     scaledPrepare(m_config.hungerPrepareMargin),
                     m_config.hungerPreferredLocator,
                     best);

        considerNeed(NeedType::Thirst,
                     world::ResourceType::Water,
                     scaledUnits(m_config.thirstUnitsPerRequest),
                     m_config.thirstReliefPerUnit,
                     scaledPrepare(m_config.thirstPrepareMargin),
                     m_config.thirstPreferredLocator,
                     best);

        considerNeed(NeedType::Social,
                     world::ResourceType::Social,
                     scaledUnits(m_config.socialUnitsPerRequest),
                     m_config.socialReliefPerUnit,
                     scaledPrepare(m_config.socialPrepareMargin),
                     m_config.socialPreferredLocator,
                     best);

        std::optional<Candidate> keep;
        if (previousTarget != 0) {
            const auto spawnIt = spawnByInteraction.find(previousTarget);
            bool keepAllowed = false;
            if (spawnIt != spawnByInteraction.end()) {
                if (spawnIt->second.current > 0U) {
                    keepAllowed = true;
                } else {
                    const auto inter = db.findInteraction(previousTarget);
                    keepAllowed = inter && isWorkshopInteraction(*inter);
                }
            }
            if (spawnIt != spawnByInteraction.end() && keepAllowed) {
                const auto& spawn = spawnIt->second;
                const auto evalKeep = [&](NeedType need,
                                         world::ResourceType resource,
                                         std::uint32_t unitsPerRequest,
                                         float reliefPerUnit,
                                         float prepareMargin) {
                    if (spawn.type != resource) {
                        return;
                    }
                    auto* state = component.needs.state(need);
                    const auto* descriptor = component.needs.descriptor(need);
                    if (!state || !descriptor) {
                        return;
                    }
                    const auto sample = ensureSample(component, need, *descriptor, *state);
                    const float prepareThreshold = descriptor->satisfiedThreshold + prepareMargin;
                    if (state->value < prepareThreshold && !sample.critical) {
                        return;
                    }
                    const float urgency = urgency01(*state, *descriptor);
                    if (urgency <= 0.0f) {
                        return;
                    }
                    if (auto candidate = buildCandidate(need,
                                                        resource,
                                                        unitsPerRequest,
                                                        reliefPerUnit,
                                                        urgency,
                                                        previousTarget,
                                                        spawn)) {
                        if (!keep || candidate->score > keep->score) {
                            keep = std::move(candidate);
                        }
                    }
                };

                evalKeep(NeedType::Hunger,
                         world::ResourceType::Food,
                         scaledUnits(m_config.hungerUnitsPerRequest),
                         m_config.hungerReliefPerUnit,
                         scaledPrepare(m_config.hungerPrepareMargin));

                evalKeep(NeedType::Thirst,
                         world::ResourceType::Water,
                         scaledUnits(m_config.thirstUnitsPerRequest),
                         m_config.thirstReliefPerUnit,
                         scaledPrepare(m_config.thirstPrepareMargin));

                evalKeep(NeedType::Social,
                         world::ResourceType::Social,
                         scaledUnits(m_config.socialUnitsPerRequest),
                         m_config.socialReliefPerUnit,
                         scaledPrepare(m_config.socialPrepareMargin));
            }
        }

        if (best && keep && best->interaction != keep->interaction) {
            if (best->score < keep->score + switchMargin) {
                best = keep;
            }
        } else if (!best && keep) {
            best = keep;
        }

        if (!best || best->interaction == 0) {
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

        if (previousTarget != 0 && previousTarget != best->interaction) {
            auto it = demandByInteraction.find(previousTarget);
            if (it != demandByInteraction.end() && it->second > 0U) {
                --it->second;
                if (it->second == 0U) {
                    demandByInteraction.erase(it);
                }
            }
        }
        if (best->interaction != 0 && best->interaction != previousTarget) {
            ++demandByInteraction[best->interaction];
        }

        registry.emplace_or_replace<components::PlannerDecision>(entity,
                                                                components::PlannerDecision{best->interaction, best->travelCost, best->score});

        if (actionExecutor) {
            actionExecutor->requestConsume(entity,
                                           best->interaction,
                                           best->need,
                                           best->resource,
                                           best->units,
                                           best->reliefPerUnit,
                                           registry);
            continue;
        }

        const auto consumed = resourceSystem.consumeFromInteraction(registry, best->interaction, best->resource, best->units);
        if (consumed == 0U) {
            continue;
        }

        if (auto* state = component.needs.state(best->need); state) {
            if (const auto* descriptor = component.needs.descriptor(best->need); descriptor) {
                const float relief = static_cast<float>(consumed) * best->reliefPerUnit;
                state->value = std::max(descriptor->minValue, state->value - relief);
                state->clamp(*descriptor);
                component.lastSamples[needIndex(best->need)] = std::nullopt;
            }
        }

    }
}

} // namespace genesis::agents
