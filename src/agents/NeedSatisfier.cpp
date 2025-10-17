#include "genesis/agents/NeedSatisfier.hpp"

#include <algorithm>
#include <optional>

#include "genesis/agents/ActionSystem.hpp"
#include "genesis/agents/AgentComponents.hpp"
#include "genesis/agents/Needs.hpp"
#include "genesis/world/WorldTypes.hpp"

namespace genesis::agents {

namespace {

NeedSample ensureSample(const NeedComponent& component, NeedType type, const NeedDescriptor& descriptor, const NeedState& state) {
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
    if (m_config.hungerPrepareThresholdOffset < 0.0f) {
        m_config.hungerPrepareThresholdOffset = 0.0f;
    }
    if (!m_config.hungerPreferredLocator) {
        m_config.hungerPreferredLocator = [](entt::entity) {
            return genesis::world::InvalidLocation;
        };
    }
}

void NeedSatisfier::update(entt::registry& registry, world::system::ResourceSystem& resourceSystem, ActionExecutor* actionExecutor) const {
    auto view = registry.view<NeedComponent>();

    for (auto entity : view) {
        auto& component = view.get<NeedComponent>(entity);
        auto* hungerState = component.needs.state(NeedType::Hunger);
        const auto* hungerDescriptor = component.needs.descriptor(NeedType::Hunger);
        if (!hungerState || !hungerDescriptor) {
            continue;
        }

        const auto hungerSample = ensureSample(component, NeedType::Hunger, *hungerDescriptor, *hungerState);
        const float prepareThreshold = std::max(
            hungerDescriptor->satisfiedThreshold,
            hungerDescriptor->criticalThreshold - m_config.hungerPrepareThresholdOffset);
        if (hungerState->value < prepareThreshold && !hungerSample.critical) {
            continue;
        }

        const auto preferredLocation = m_config.hungerPreferredLocator(entity);
        if (preferredLocation == world::InvalidLocation) {
            continue;
        }

        if (actionExecutor) {
            actionExecutor->requestConsume(entity, preferredLocation, world::ResourceType::Food, m_config.hungerUnitsPerRequest, m_config.hungerReliefPerUnit, registry);
            continue;
        }

        auto* location = registry.try_get<components::AgentLocation>(entity);
        if (!location) {
            location = &registry.emplace<components::AgentLocation>(entity);
        }

        if (location->location != preferredLocation) {
            auto* intent = registry.try_get<components::MovementIntent>(entity);
            if (!intent) {
                intent = &registry.emplace<components::MovementIntent>(entity);
            }
            if (intent->target != preferredLocation) {
                intent->target = preferredLocation;
            }
            if (intent->speed <= 0.0f) {
                intent->speed = 1.0f;
            }
            continue;
        }

        const auto consumed = resourceSystem.consume(registry, world::ResourceType::Food, m_config.hungerUnitsPerRequest, preferredLocation);
        if (consumed == 0U) {
            continue;
        }

        const float relief = static_cast<float>(consumed) * m_config.hungerReliefPerUnit;
        hungerState->value = std::max(hungerDescriptor->minValue, hungerState->value - relief);
        hungerState->clamp(*hungerDescriptor);

        component.lastSamples[needIndex(NeedType::Hunger)] = std::nullopt;
    }
}

} // namespace genesis::agents
