#pragma once

#include <entt/entt.hpp>

namespace genesis::simulation {

class Movement2DSystem;

class Scheduler {
public:
    explicit Scheduler(const Movement2DSystem* movement) : movement_(movement) {}
    void update(entt::registry& registry, float deltaSeconds) const;
private:
    const Movement2DSystem* movement_{nullptr};
};

} // namespace genesis::simulation

