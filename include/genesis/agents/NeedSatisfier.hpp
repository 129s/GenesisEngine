#pragma once

#include <cstdint>
#include <functional>

#include <entt/entt.hpp>

#include "genesis/agents/NeedSystem.hpp"
#include "genesis/world/system/ResourceSystem.hpp"
#include "genesis/world/WorldDatabase.hpp"

namespace genesis::agents {

class ActionExecutor;

struct NeedSatisfierConfig {
    std::uint32_t hungerUnitsPerRequest{2};
    float hungerReliefPerUnit{12.0f};
    float hungerPrepareMargin{5.0f};
    std::function<world::InteractionId(entt::entity)> hungerPreferredLocator{};
};

class NeedSatisfier {
public:
    explicit NeedSatisfier(NeedSatisfierConfig config = {});

    void update(entt::registry& registry, world::system::ResourceSystem& resourceSystem, ActionExecutor* actionExecutor = nullptr) const;

private:
    NeedSatisfierConfig m_config;
};

} // namespace genesis::agents
