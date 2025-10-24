#pragma once

#include <entt/entt.hpp>

namespace genesis::simulation {

class Movement2DSystem;
class ResourceSystem2D;

class Scheduler {
public:
    explicit Scheduler(const Movement2DSystem* movement, const ResourceSystem2D* resource = nullptr)
        : movement_(movement), resource_(resource) {}
    void update(entt::registry& registry, float deltaSeconds) const;
private:
    const Movement2DSystem* movement_{nullptr};
    const ResourceSystem2D* resource_{nullptr};
};

} // namespace genesis::simulation
