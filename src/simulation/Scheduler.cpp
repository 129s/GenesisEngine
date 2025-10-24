#include "genesis/simulation/Scheduler.hpp"

#include "genesis/simulation/Movement2DSystem.hpp"
#include "genesis/simulation/ResourceSystem2D.hpp"

namespace genesis::simulation {

void Scheduler::update(entt::registry& registry, float deltaSeconds) const {
    if (movement_) movement_->update(registry, deltaSeconds);
    if (resource_) const_cast<ResourceSystem2D*>(resource_)->update();
}

} // namespace genesis::simulation
