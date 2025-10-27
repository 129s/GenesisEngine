#include "genesis/simulation/SimulationContext.hpp"

#include <utility>

#include "genesis/telemetry/TelemetryBuffer.hpp"
#include "genesis/world/components/ResourceInventory.hpp"
#include "genesis/world/components/ResourceSpawn.hpp"

namespace genesis::simulation {

SimulationContext::SimulationContext()
    : m_needSatisfier({})
    , m_scheduler() {
    bindScheduler();
}

void SimulationContext::bindScheduler() {
    m_scheduler.setNeedSystem(&m_needSystem);
    m_scheduler.setNeedSatisfier(&m_needSatisfier);
    m_scheduler.setMovementSystem(&m_movementSystem);
    m_scheduler.setActionExecutor(m_actionExecutor.get());
    m_scheduler.setResourceSystem(m_resourceSystem.get());
}

void SimulationContext::setWorldDatabase(std::shared_ptr<world::WorldDatabase> database, entt::registry& registry) {
    if (m_resourceSystem) {
        m_resourceSystem->reset(registry);
    }
    m_actionExecutor.reset();
    m_resourceSystem.reset();
    m_worldDatabase = std::move(database);

    if (!m_worldDatabase) {
        bindScheduler();
        return;
    }

    m_resourceSystem = std::make_unique<world::system::ResourceSystem>(*m_worldDatabase, m_eventBus);
    m_resourceSystem->initialize(registry);
    m_actionExecutor = std::make_unique<agents::ActionExecutor>(*m_worldDatabase, *m_resourceSystem);
    bindScheduler();
}

void SimulationContext::tick(entt::registry& registry, float deltaSeconds, std::uint64_t stepIndex) {
    m_scheduler.update(registry, deltaSeconds, stepIndex);
}

void SimulationContext::reset(entt::registry& registry) {
    if (m_resourceSystem) {
        m_resourceSystem->reset(registry);
    }
    m_actionExecutor.reset();
    m_resourceSystem.reset();
    m_worldDatabase.reset();
    bindScheduler();
}

void SimulationContext::collectResourceSnapshots(const entt::registry& registry,
                                                 std::vector<telemetry::ResourceSnapshot>& out) const {
    out.clear();
    if (!m_resourceSystem || !m_worldDatabase || !m_resourceSystem->m_initialized) {
        return;
    }

    out.reserve(m_resourceSystem->m_spawnEntities.size());
    for (const auto entity : m_resourceSystem->m_spawnEntities) {
        if (!registry.valid(entity)) {
            continue;
        }
        const auto& inventory = registry.get<world::components::ResourceInventory>(entity);
        const auto& spawn = registry.get<world::components::ResourceSpawn>(entity);
        telemetry::ResourceSnapshot snapshot{};
        snapshot.interactionId = spawn.interaction;
        snapshot.mapId = spawn.mapId;
        snapshot.name = spawn.name;
        snapshot.type = spawn.type;
        snapshot.capacity = inventory.capacity;
        snapshot.current = inventory.current;
        out.push_back(std::move(snapshot));
    }
}

} // namespace genesis::simulation

