#pragma once

#include <array>
#include <optional>

#include <entt/entt.hpp>

#include "genesis/agents/Needs.hpp"

namespace genesis::agents {

struct NeedComponent {
    NeedCollection needs;
    std::array<std::optional<NeedSample>, needTypeCount()> lastSamples{};
};

class NeedSystem {
public:
    NeedSystem();

    void update(entt::registry& registry, float deltaSeconds);

    void applyDefaults(NeedComponent& component) const;

    void setDefaultDescriptor(const NeedDescriptor& descriptor);

private:
    NeedRegulator m_regulator{};
    std::array<std::optional<NeedDescriptor>, needTypeCount()> m_defaultDescriptors{};
};

} // namespace genesis::agents
