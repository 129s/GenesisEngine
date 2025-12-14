#include "genesis/agents/NeedSatisfier.hpp"

#include <algorithm>
#include <optional>
#include <utility>

#include "genesis/agents/ActionSystem.hpp"
#include "genesis/agents/Needs.hpp"
#include "genesis/agents/Movement2D.hpp"

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

        const auto findNearest = [&](world::ResourceType type) -> world::InteractionId {
            struct Candidate {
                world::InteractionId id{0};
                float cost{0.0f};
            };
            std::optional<Candidate> best;

            resourceSystem.forEachSpawn(registry, [&](const auto& spawn, const auto& inventory) {
                if (spawn.type != type) {
                    return;
                }
                if (inventory.current == 0U) {
                    return;
                }
                const auto inter = db.findInteraction(spawn.interaction);
                if (!inter) {
                    return;
                }

                const float dx = static_cast<float>(inter->coord.first) - location.x;
                const float dy = static_cast<float>(inter->coord.second) - location.y;
                const float dist2 = dx * dx + dy * dy;
                const float mapPenalty = (inter->mapId == location.mapId) ? 0.0f : 1000.0f;
                const float cost = mapPenalty + dist2;

                if (!best || cost < best->cost) {
                    best = Candidate{spawn.interaction, cost};
                }
            });

            return best ? best->id : world::InteractionId{0};
        };

        const auto trySatisfy = [&](NeedType need,
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

            const auto sample = ensureSample(component, need, *descriptor, *state);
            const float prepareThreshold = descriptor->satisfiedThreshold + prepareMargin;
            if (state->value < prepareThreshold && !sample.critical) {
                return;
            }

            world::InteractionId interaction{0};
            if (preferredLocator) {
                interaction = preferredLocator(entity);
            }
            if (interaction == 0) {
                interaction = findNearest(resource);
            }
            if (interaction == 0) {
                return;
            }

            if (actionExecutor) {
                actionExecutor->requestConsume(entity, interaction, need, resource, unitsPerRequest, reliefPerUnit, registry);
                return;
            }

            const auto consumed = resourceSystem.consume(registry, resource, unitsPerRequest, interaction);
            if (consumed == 0U) {
                return;
            }

            const float relief = static_cast<float>(consumed) * reliefPerUnit;
            state->value = std::max(descriptor->minValue, state->value - relief);
            state->clamp(*descriptor);
            component.lastSamples[needIndex(need)] = std::nullopt;
        };

        trySatisfy(NeedType::Hunger,
                   world::ResourceType::Food,
                   m_config.hungerUnitsPerRequest,
                   m_config.hungerReliefPerUnit,
                   m_config.hungerPrepareMargin,
                   m_config.hungerPreferredLocator);

        trySatisfy(NeedType::Thirst,
                   world::ResourceType::Drink,
                   m_config.thirstUnitsPerRequest,
                   m_config.thirstReliefPerUnit,
                   m_config.thirstPrepareMargin,
                   m_config.thirstPreferredLocator);

        trySatisfy(NeedType::Social,
                   world::ResourceType::Social,
                   m_config.socialUnitsPerRequest,
                   m_config.socialReliefPerUnit,
                   m_config.socialPrepareMargin,
                   m_config.socialPreferredLocator);

    }
}

} // namespace genesis::agents
