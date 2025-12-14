#include "genesis/agents/NeedSatisfier.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
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
            const float mapPenalty = (inter.mapId == location.mapId) ? 0.0f : 1000.0f;
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
                if (preferred != 0) {
                    if (const auto inter = db.findInteraction(preferred); inter && inter->kind == world::InteractionKind::Resource) {
                        if (inter->resourceType.value_or(resource) == resource) {
                            Candidate c{};
                            c.need = need;
                            c.resource = resource;
                            c.interaction = preferred;
                            c.travelCost = costToInteraction(*inter);
                            c.units = unitsPerRequest;
                            c.reliefPerUnit = reliefPerUnit;
                            c.score = urgency * 1000.0f - c.travelCost;
                            if (!best || c.score > best->score) {
                                best = c;
                            }
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
                const auto inter = db.findInteraction(spawn.interaction);
                if (!inter) {
                    return;
                }

                const float travelCost = costToInteraction(*inter);
                const float scarcity01 =
                    (inventory.capacity == 0U) ? 1.0f : (1.0f - static_cast<float>(inventory.current) / static_cast<float>(inventory.capacity));
                const float scarcityPenalty = scarcity01 * 50.0f;

                Candidate c{};
                c.need = need;
                c.resource = resource;
                c.interaction = spawn.interaction;
                c.travelCost = travelCost;
                c.units = unitsPerRequest;
                c.reliefPerUnit = reliefPerUnit;
                c.score = urgency * 1000.0f - travelCost - scarcityPenalty;
                if (!best || c.score > best->score) {
                    best = c;
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

        if (!best || best->interaction == 0) {
            if (registry.any_of<components::PlannerDecision>(entity)) {
                registry.remove<components::PlannerDecision>(entity);
            }
            continue;
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
