#include "genesis/agents/NeedSatisfier.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>

#include "genesis/agents/ActionSystem.hpp"
#include "genesis/agents/Needs.hpp"
#include "genesis/agents/Movement2D.hpp"
#include "genesis/agents/Planner.hpp"

namespace genesis::agents {

namespace {

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

    for (auto entity : view) {
        auto& component = view.get<NeedComponent>(entity);
        const auto& location = view.get<components::AgentLocation2D>(entity);

        if (actionExecutor && actionExecutor->hasPendingActions(entity, registry)) {
            continue;
        }
        if (registry.any_of<components::MovementIntent2D>(entity)) {
            continue;
        }

        const auto costToInteraction = [&](const world::Interaction& inter) -> float {
            const float dx = static_cast<float>(inter.coord.first) - location.x;
            const float dy = static_cast<float>(inter.coord.second) - location.y;
            const float dist2 = dx * dx + dy * dy;
            const float mapPenalty = (inter.mapId == location.mapId) ? 0.0f : std::max(0.0f, m_config.crossMapPenalty);
            return mapPenalty + dist2;
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
            if (spawnInfo.type != resource || spawnInfo.current == 0U) {
                return std::nullopt;
            }
            const auto inter = db.findInteraction(interaction);
            if (!inter || inter->kind != world::InteractionKind::Resource) {
                return std::nullopt;
            }

            const float travelCost = costToInteraction(*inter);
            const float scarcity01 = (spawnInfo.capacity == 0U)
                ? 1.0f
                : (1.0f - static_cast<float>(spawnInfo.current) / static_cast<float>(spawnInfo.capacity));
            const float scarcityPenalty = scarcity01 * 50.0f;
            const float crowdPenalty = demandPenalty(interaction, previousTarget);

            Candidate c{};
            c.need = need;
            c.resource = resource;
            c.interaction = interaction;
            c.travelCost = travelCost;
            c.units = unitsPerRequest;
            c.reliefPerUnit = reliefPerUnit;
            c.score = urgency * 1000.0f - travelCost - scarcityPenalty - crowdPenalty;
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
                    return;
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
                     m_config.hungerUnitsPerRequest,
                     m_config.hungerReliefPerUnit,
                     m_config.hungerPrepareMargin,
                     m_config.hungerPreferredLocator,
                     best);

        considerNeed(NeedType::Thirst,
                     world::ResourceType::Drink,
                     m_config.thirstUnitsPerRequest,
                     m_config.thirstReliefPerUnit,
                     m_config.thirstPrepareMargin,
                     m_config.thirstPreferredLocator,
                     best);

        considerNeed(NeedType::Social,
                     world::ResourceType::Social,
                     m_config.socialUnitsPerRequest,
                     m_config.socialReliefPerUnit,
                     m_config.socialPrepareMargin,
                     m_config.socialPreferredLocator,
                     best);

        std::optional<Candidate> keep;
        if (previousTarget != 0) {
            const auto spawnIt = spawnByInteraction.find(previousTarget);
            if (spawnIt != spawnByInteraction.end() && spawnIt->second.current > 0U) {
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
                         m_config.hungerUnitsPerRequest,
                         m_config.hungerReliefPerUnit,
                         m_config.hungerPrepareMargin);

                evalKeep(NeedType::Thirst,
                         world::ResourceType::Drink,
                         m_config.thirstUnitsPerRequest,
                         m_config.thirstReliefPerUnit,
                         m_config.thirstPrepareMargin);

                evalKeep(NeedType::Social,
                         world::ResourceType::Social,
                         m_config.socialUnitsPerRequest,
                         m_config.socialReliefPerUnit,
                         m_config.socialPrepareMargin);
            }
        }

        if (best && keep && best->interaction != keep->interaction) {
            if (best->score < keep->score + std::max(0.0f, m_config.switchScoreMargin)) {
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

        const auto consumed = resourceSystem.consume(registry, best->resource, best->units, best->interaction);
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
