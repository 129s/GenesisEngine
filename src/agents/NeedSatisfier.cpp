#include "genesis/agents/NeedSatisfier.hpp"

#include <algorithm>
#include <optional>

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
    if (!m_config.hungerPreferredLocator) {
        m_config.hungerPreferredLocator = [](entt::entity) {
            return genesis::world::InvalidLocation;
        };
    }
}

void NeedSatisfier::update(entt::registry& registry, world::system::ResourceSystem& resourceSystem) const {
    auto view = registry.view<NeedComponent>();

    for (auto entity : view) {
        auto& component = view.get<NeedComponent>(entity);
        auto* hungerState = component.needs.state(NeedType::Hunger);
        const auto* hungerDescriptor = component.needs.descriptor(NeedType::Hunger);
        if (!hungerState || !hungerDescriptor) {
            continue;
        }

        const auto hungerSample = ensureSample(component, NeedType::Hunger, *hungerDescriptor, *hungerState);
        if (!hungerSample.critical) {
            continue;
        }

        const auto preferredLocation = m_config.hungerPreferredLocator(entity);
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
