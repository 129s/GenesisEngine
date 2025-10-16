#pragma once

#include <cstdint>

#include <entt/entt.hpp>

#include "genesis/agents/NeedSystem.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

namespace genesis::agents {

struct NeedSatisfierConfig {
    std::uint32_t hungerUnitsPerRequest{2};
    float hungerReliefPerUnit{12.0f};
};

class NeedSatisfier {
public:
    explicit NeedSatisfier(NeedSatisfierConfig config = {});

    void update(entt::registry& registry, world::system::ResourceSystem& resourceSystem) const;

private:
    NeedSatisfierConfig m_config;
};

} // namespace genesis::agents
