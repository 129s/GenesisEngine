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

std::uint32_t SimulationHost::createAgent(const AgentSpawnParams2D& params) {
    const auto entity = m_registry.create();
    agents::components::AgentLocation2D location{};
    location.mapId = params.location.mapId;
    location.x = params.location.x;
    location.y = params.location.y;
    m_registry.emplace<agents::components::AgentLocation2D>(entity, location);

    if (params.initialMovement) {
        agents::components::MovementIntent2D intent{};
        intent.targetMapId = params.initialMovement->targetMapId;
        intent.targetX = params.initialMovement->targetX;
        intent.targetY = params.initialMovement->targetY;
        intent.speed = params.initialMovement->speed;
        m_registry.emplace<agents::components::MovementIntent2D>(entity, intent);
    }
    return static_cast<std::uint32_t>(entt::to_integral(entity));
}

bool SimulationHost::setAgentMovementIntent(std::uint32_t entityId, const MovementCommand2D& command) {
    const auto entity = toEntity(entityId);
    if (!m_registry.valid(entity) || !m_registry.all_of<agents::components::AgentLocation2D>(entity)) {
        return false;
    }
    if (auto* existing = m_registry.try_get<agents::components::MovementIntent2D>(entity)) {
        existing->targetMapId = command.targetMapId;
        existing->targetX = command.targetX;
        existing->targetY = command.targetY;
        existing->speed = command.speed;
    } else {
        agents::components::MovementIntent2D intent{};
        intent.targetMapId = command.targetMapId;
        intent.targetX = command.targetX;
        intent.targetY = command.targetY;
        intent.speed = command.speed;
        m_registry.emplace<agents::components::MovementIntent2D>(entity, intent);
    }
    return true;
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
    const auto entity = toEntity(entityId);
    if (!m_registry.valid(entity)) {
        return false;
    }
    if (m_registry.any_of<agents::components::MovementIntent2D>(entity)) {
        m_registry.remove<agents::components::MovementIntent2D>(entity);
    }
    return true;
}

bool SimulationHost::teleportAgent(std::uint32_t entityId, const AgentPose2D& target) {
    const auto entity = toEntity(entityId);
    if (!m_registry.valid(entity)) {
        return false;
    }
    auto& loc = m_registry.get_or_emplace<agents::components::AgentLocation2D>(entity);
    loc.mapId = target.mapId;
    loc.x = target.x;
    loc.y = target.y;
    if (m_registry.any_of<agents::components::MovementIntent2D>(entity)) {
        m_registry.remove<agents::components::MovementIntent2D>(entity);
    }
    return true;
}

bool SimulationHost::teleportAgent(std::uint32_t entityId, const agents::components::AgentLocation2D& target) {
    AgentPose2D pose{};
    pose.mapId = target.mapId;
    pose.x = target.x;
    pose.y = target.y;
    return teleportAgent(entityId, pose);
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
    const auto entity = toEntity(entityId);
    if (!m_registry.valid(entity) || !m_registry.all_of<agents::components::AgentLocation2D>(entity)) {
        return std::nullopt;
    }
    const auto& loc = m_registry.get<agents::components::AgentLocation2D>(entity);
    AgentPose2D pose{};
    pose.mapId = loc.mapId;
    pose.x = loc.x;
    pose.y = loc.y;
    return pose;
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
        AgentSpawnParams2D params{};
        params.location.mapId = 1;
        params.location.x = 0.0f;
        params.location.y = 0.0f;
        MovementCommand2D intent{};
        intent.targetMapId = 1;
        intent.targetX = 10.0f;
        intent.targetY = 5.0f;
        intent.speed = 2.0f;
        params.initialMovement = intent;
        (void)createAgent(params);
    }
}

entt::entity SimulationHost::toEntity(std::uint32_t id) const noexcept {
    return static_cast<entt::entity>(id);
}

} // namespace genesis::simulation
