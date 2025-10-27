#include "genesis/simulation/SimulationHost.hpp"

#include <algorithm>
#include <optional>
#include <utility>

#include "genesis/world/WorldDatabase.hpp"

namespace genesis::simulation {

SimulationHost::SimulationHost()
    : m_context()
    , m_telemetryCollector()
    , m_resourceScratch() {}

void SimulationHost::reset() {
    m_context.reset(m_registry);
    m_registry.clear();
    m_resourceScratch.clear();
    m_worldDb.reset();
}

void SimulationHost::setWorldDatabase(std::shared_ptr<world::WorldDatabase> database) {
    reset();
    m_worldDb = std::move(database);
    if (m_worldDb) {
        m_context.setWorldDatabase(m_worldDb, m_registry);
    }
}

std::shared_ptr<world::WorldDatabase> SimulationHost::worldDatabase() const noexcept {
    return m_worldDb;
}

void SimulationHost::tick(float deltaSeconds, std::uint64_t stepIndex) {
    m_context.tick(m_registry, deltaSeconds, stepIndex);
}

telemetry::TickTelemetry SimulationHost::captureTelemetry(std::uint64_t stepIndex, float stepSeconds) {
    m_context.collectResourceSnapshots(m_registry, m_resourceScratch);
    return m_telemetryCollector.collect(
        m_registry,
        m_worldDb ? m_worldDb.get() : nullptr,
        m_resourceScratch,
        stepIndex,
        stepSeconds);
}

std::uint32_t SimulationHost::createAgent2D(const agents::components::AgentLocation2D& location,
                                            const std::optional<agents::components::MovementIntent2D>& intent) {
    const auto entity = m_registry.create();
    m_registry.emplace<agents::components::AgentLocation2D>(entity, location);
    if (intent) {
        m_registry.emplace<agents::components::MovementIntent2D>(entity, *intent);
    }
    return static_cast<std::uint32_t>(entt::to_integral(entity));
}

bool SimulationHost::setAgentMovementIntent(std::uint32_t entityId,
                                            const agents::components::MovementIntent2D& intent) {
    const auto entity = toEntity(entityId);
    if (!m_registry.valid(entity) || !m_registry.all_of<agents::components::AgentLocation2D>(entity)) {
        return false;
    }
    if (auto* existing = m_registry.try_get<agents::components::MovementIntent2D>(entity)) {
        *existing = intent;
    } else {
        m_registry.emplace<agents::components::MovementIntent2D>(entity, intent);
    }
    return true;
}

bool SimulationHost::clearAgentMovementIntent(std::uint32_t entityId) {
    const auto entity = toEntity(entityId);
    if (!m_registry.valid(entity)) {
        return false;
    }
    if (m_registry.any_of<agents::components::MovementIntent2D>(entity)) {
        m_registry.remove<agents::components::MovementIntent2D>(entity);
    }
    return true;
}

bool SimulationHost::teleportAgent(std::uint32_t entityId, const agents::components::AgentLocation2D& target) {
    const auto entity = toEntity(entityId);
    if (!m_registry.valid(entity)) {
        return false;
    }
    auto& loc = m_registry.get_or_emplace<agents::components::AgentLocation2D>(entity);
    loc = target;
    if (m_registry.any_of<agents::components::MovementIntent2D>(entity)) {
        m_registry.remove<agents::components::MovementIntent2D>(entity);
    }
    return true;
}

bool SimulationHost::deleteAgent(std::uint32_t entityId) {
    const auto entity = toEntity(entityId);
    if (!m_registry.valid(entity)) {
        return false;
    }
    m_registry.destroy(entity);
    return true;
}

bool SimulationHost::agentExists(std::uint32_t entityId) const {
    const auto entity = toEntity(entityId);
    return m_registry.valid(entity) && m_registry.all_of<agents::components::AgentLocation2D>(entity);
}

std::optional<agents::components::AgentLocation2D> SimulationHost::queryAgentLocation(std::uint32_t entityId) const {
    const auto entity = toEntity(entityId);
    if (!m_registry.valid(entity) || !m_registry.all_of<agents::components::AgentLocation2D>(entity)) {
        return std::nullopt;
    }
    return m_registry.get<agents::components::AgentLocation2D>(entity);
}

std::uint32_t SimulationHost::consumeResource(std::uint32_t interactionId, std::uint32_t amount) {
    auto* resourceSystem = m_context.resourceSystem();
    if (!resourceSystem) {
        return 0;
    }
    return resourceSystem->consume(m_registry, world::ResourceType::Food, amount, interactionId);
}

void SimulationHost::spawnDemoAgentsIfEmpty() {
    if (m_registry.view<agents::components::AgentLocation2D>().empty()) {
        agents::components::AgentLocation2D loc{};
        loc.mapId = 1;
        loc.x = 0.0f;
        loc.y = 0.0f;

        agents::components::MovementIntent2D intent{};
        intent.targetMapId = 1;
        intent.targetX = 10.0f;
        intent.targetY = 5.0f;
        intent.speed = 2.0f;

        (void)createAgent2D(loc, std::optional{intent});
    }
}

entt::entity SimulationHost::toEntity(std::uint32_t id) const noexcept {
    return static_cast<entt::entity>(id);
}

} // namespace genesis::simulation
