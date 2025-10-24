#pragma once

#include <entt/entt.hpp>

namespace genesis::simulation {

class Movement2DSystem {
public:
    void update(entt::registry& registry, float deltaSeconds) const;
};

} // namespace genesis::simulation

