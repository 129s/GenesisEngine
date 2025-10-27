#include "genesis/simulation/Movement2DSystem.hpp"

#include <cmath>

#include "genesis/agents/Movement2D.hpp"

namespace genesis::simulation {

void Movement2DSystem::update(entt::registry& registry, float deltaSeconds) const {
    auto view = registry.view<genesis::agents::components::AgentLocation2D, genesis::agents::components::MovementIntent2D>();
    view.each([&](auto entity, auto& loc, auto& intent) {
        if (intent.targetMapId != loc.mapId) {
            loc.mapId = intent.targetMapId;
            loc.x = intent.targetX;
            loc.y = intent.targetY;
            registry.remove<genesis::agents::components::MovementIntent2D>(entity);
            return;
        }
        const float dx = intent.targetX - loc.x;
        const float dy = intent.targetY - loc.y;
        const float dist2 = dx * dx + dy * dy;
        const float speed = std::max(intent.speed, 0.0f);
        if (dist2 <= 1e-6f || speed <= 0.0f) {
            loc.x = intent.targetX;
            loc.y = intent.targetY;
            registry.remove<genesis::agents::components::MovementIntent2D>(entity);
            return;
        }
        const float dist = std::sqrt(dist2);
        const float step = speed * deltaSeconds;
        if (step >= dist) {
            loc.x = intent.targetX;
            loc.y = intent.targetY;
            registry.remove<genesis::agents::components::MovementIntent2D>(entity);
        } else {
            loc.x += dx / dist * step;
            loc.y += dy / dist * step;
        }
    });
}

} // namespace genesis::simulation

