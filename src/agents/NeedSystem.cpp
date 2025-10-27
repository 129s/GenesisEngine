#include "genesis/agents/NeedSystem.hpp"

namespace genesis::agents {

namespace {

void storeSample(NeedComponent& component, const NeedSample& sample) {
    component.lastSamples[needIndex(sample.type)] = sample;
}

} // namespace

NeedSystem::NeedSystem() = default;

void NeedSystem::update(entt::registry& registry, float deltaSeconds) {
    auto view = registry.view<NeedComponent>();

    view.each([this, deltaSeconds](NeedComponent& component) {
        component.needs.forEach([this, deltaSeconds, &component](NeedState& state, const NeedDescriptor& descriptor) {
            const auto sample = m_regulator.update(deltaSeconds, state, descriptor);
            storeSample(component, sample);
        });
    });
}

void NeedSystem::applyDefaults(NeedComponent& component) const {
    for (std::size_t idx = 0; idx < needTypeCount(); ++idx) {
        if (m_defaultDescriptors[idx].has_value()) {
            component.needs.setDescriptor(*m_defaultDescriptors[idx]);
        }
    }
}

void NeedSystem::setDefaultDescriptor(const NeedDescriptor& descriptor) {
    m_defaultDescriptors[needIndex(descriptor.type)] = descriptor;
}

} // namespace genesis::agents

