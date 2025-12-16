#include "genesis/simulation/Scheduler.hpp"

#include "genesis/agents/ActionSystem.hpp"
#include "genesis/agents/NeedSatisfier.hpp"
#include "genesis/agents/NeedSystem.hpp"
#include "genesis/simulation/Movement2DSystem.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

namespace genesis::simulation {

void Scheduler::update(entt::registry& registry, float deltaSeconds, std::uint64_t stepIndex) const {
    if (m_needSystem) {
        m_needSystem->update(registry, deltaSeconds);
    }
    if (m_needSatisfier && m_resourceSystem && m_worldDatabase) {
        m_needSatisfier->update(registry, *m_worldDatabase, *m_resourceSystem, stepIndex, m_actionExecutor);
    }
    if (m_actionExecutor) {
        m_actionExecutor->update(registry, deltaSeconds);
    }
    if (m_movementSystem) {
        m_movementSystem->update(registry, deltaSeconds);
    }
    if (m_resourceSystem) {
        m_resourceSystem->tick(registry, stepIndex);
    }
}

} // namespace genesis::simulation
