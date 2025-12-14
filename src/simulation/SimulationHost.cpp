#include "genesis/simulation/SimulationHost.hpp"

#include <optional>
#include <span>
#include <utility>

#include "genesis/world/WorldDatabase.hpp"

namespace genesis::simulation {

SimulationHost::SimulationHost()
    : m_context()
    , m_telemetryCollector()
    , m_agentScratch()
    , m_resourceScratch()
    , m_needScratch()
    , m_actionScratch() {}

void SimulationHost::reset() {
    m_context.reset();
    m_agentScratch.clear();
    m_resourceScratch.clear();
    m_needScratch.clear();
    m_actionScratch.clear();
    m_worldDb.reset();
}

void SimulationHost::setWorldDatabase(std::shared_ptr<world::WorldDatabase> database) {
    if (!database) {
        reset();
        return;
    }
    m_worldDb = std::move(database);
    m_context.setWorldDatabase(m_worldDb);
    m_agentScratch.clear();
    m_resourceScratch.clear();
    m_needScratch.clear();
    m_actionScratch.clear();
}

std::shared_ptr<world::WorldDatabase> SimulationHost::worldDatabase() const noexcept {
    return m_worldDb;
}

void SimulationHost::tick(float deltaSeconds, std::uint64_t stepIndex) {
    m_context.tick(deltaSeconds, stepIndex);
}

telemetry::TickTelemetry SimulationHost::captureTelemetry(std::uint64_t stepIndex, float stepSeconds) {
    m_context.collectAgentSnapshots(m_agentScratch);
    m_context.collectResourceSnapshots(m_resourceScratch);
    m_context.collectNeedSnapshots(m_needScratch);
    m_context.collectActionSnapshots(m_actionScratch);
    return m_telemetryCollector.collect(
        std::span<const telemetry::AgentSnapshot>(m_agentScratch),
        m_worldDb ? m_worldDb.get() : nullptr,
        std::span<const telemetry::ResourceSnapshot>(m_resourceScratch),
        std::span<const telemetry::NeedSnapshot>(m_needScratch),
        std::span<const telemetry::ActionSnapshot>(m_actionScratch),
        stepIndex,
        stepSeconds);
}

std::uint32_t SimulationHost::createAgent(const AgentSpawnParams2D& params) {
    return m_context.createAgent(params);
}

bool SimulationHost::setAgentMovementIntent(std::uint32_t entityId, const MovementCommand2D& command) {
    return m_context.setAgentMovementIntent(entityId, command);
}

std::uint32_t SimulationHost::createAgent2D(const agents::components::AgentLocation2D& location,
                                            const std::optional<agents::components::MovementIntent2D>& intent) {
    AgentSpawnParams2D params{};
    params.location.mapId = location.mapId;
    params.location.x = location.x;
    params.location.y = location.y;
    if (intent) {
        MovementCommand2D command{};
        command.targetMapId = intent->targetMapId;
        command.targetX = intent->targetX;
        command.targetY = intent->targetY;
        command.speed = intent->speed;
        params.initialMovement = command;
    }
    return createAgent(params);
}

bool SimulationHost::setAgentMovementIntent(std::uint32_t entityId,
                                            const agents::components::MovementIntent2D& intent) {
    MovementCommand2D command{};
    command.targetMapId = intent.targetMapId;
    command.targetX = intent.targetX;
    command.targetY = intent.targetY;
    command.speed = intent.speed;
    return setAgentMovementIntent(entityId, command);
}

bool SimulationHost::clearAgentMovementIntent(std::uint32_t entityId) {
    return m_context.clearAgentMovementIntent(entityId);
}

bool SimulationHost::teleportAgent(std::uint32_t entityId, const AgentPose2D& target) {
    return m_context.teleportAgent(entityId, target);
}

bool SimulationHost::teleportAgent(std::uint32_t entityId, const agents::components::AgentLocation2D& target) {
    AgentPose2D pose{};
    pose.mapId = target.mapId;
    pose.x = target.x;
    pose.y = target.y;
    return teleportAgent(entityId, pose);
}

bool SimulationHost::deleteAgent(std::uint32_t entityId) {
    return m_context.deleteAgent(entityId);
}

bool SimulationHost::agentExists(std::uint32_t entityId) const {
    return m_context.agentExists(entityId);
}

std::optional<agents::components::AgentLocation2D> SimulationHost::queryAgentLocation(std::uint32_t entityId) const {
    auto pose = queryAgentPose(entityId);
    if (!pose) {
        return std::nullopt;
    }
    agents::components::AgentLocation2D location{};
    location.mapId = pose->mapId;
    location.x = pose->x;
    location.y = pose->y;
    return location;
}

std::optional<AgentPose2D> SimulationHost::queryAgentPose(std::uint32_t entityId) const {
    return m_context.queryAgentPose(entityId);
}

std::uint32_t SimulationHost::consumeResource(std::uint32_t interactionId, std::uint32_t amount) {
    return m_context.consumeResource(interactionId, amount);
}

void SimulationHost::spawnDemoAgentsIfEmpty() {
    m_context.spawnDemoAgentsIfEmpty();
}

} // namespace genesis::simulation
