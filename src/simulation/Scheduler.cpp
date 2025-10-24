#include "genesis/simulation/Scheduler.hpp"

#include "genesis/simulation/Movement2DSystem.hpp"

namespace genesis::simulation {

void Scheduler::update(entt::registry& registry, float deltaSeconds) const {
    if (movement_) movement_->update(registry, deltaSeconds);
}

} // namespace genesis::simulation

