#pragma once

#include <cstdint>
#include <functional>

#include <entt/entt.hpp>

#include "genesis/agents/NeedSystem.hpp"
#include "genesis/world/system/ResourceSystem.hpp"
#include "genesis/world/WorldTypes.hpp"

namespace genesis::agents {

struct NeedSatisfierConfig {
    std::uint32_t hungerUnitsPerRequest{2};
    float hungerReliefPerUnit{12.0f};
    std::function<world::LocationId(entt::entity)> hungerPreferredLocator{};
};

class NeedSatisfier {
public:
    explicit NeedSatisfier(NeedSatisfierConfig config = {});

    void update(entt::registry& registry, world::system::ResourceSystem& resourceSystem) const;

private:
    NeedSatisfierConfig m_config;
};

} // namespace genesis::agents
