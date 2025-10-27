#include "genesis/core/Engine.hpp"

#include <chrono>

#include <spdlog/spdlog.h>

#include "genesis/world/WorldDatabaseLoader.hpp"

namespace genesis::core {

Engine::Engine()
    : m_clock(SimulationClock::duration{500})
    , m_telemetry(512) {
    spdlog::info("GenesisEngine v2 core initialized (2D sim)");
}

void Engine::setSnapshotCallback(SnapshotCallback callback) {
    m_snapshotCallback = std::move(callback);
}

void Engine::run(std::uint64_t maxSteps) {
    const auto delta = m_clock.stepDuration();
    std::uint64_t processed = 0;
    while (processed < maxSteps) {
        m_clock.advance(delta);
        if (!m_clock.stepReady()) continue;
        const auto current = m_clock.consumeStep();
        processStep(current);
        ++processed;
    }
}

void Engine::step(std::uint64_t steps) {
    const auto delta = m_clock.stepDuration();
    for (std::uint64_t processed = 0; processed < steps; ++processed) {
        m_clock.advance(delta);
        while (m_clock.stepReady()) {
            const auto current = m_clock.consumeStep();
            processStep(current);
        }
    }
}

void Engine::processStep(std::uint64_t stepIndex) {
    const float deltaSeconds = std::chrono::duration<float>(m_clock.stepDuration()).count();
    m_host.tick(deltaSeconds, stepIndex);
    captureTelemetry(stepIndex);
}

genesis::world::WorldDbLoadResult Engine::loadWorldFromFile(const std::filesystem::path& folder) {
    auto res = genesis::world::loadWorldDatabaseFromFolder(folder);
    if (res.success && res.database) {
        m_host.setWorldDatabase(res.database);
        m_host.spawnDemoAgentsIfEmpty();
    }
    return res;
}

void Engine::captureTelemetry(std::uint64_t stepIndex) {
    const float stepSeconds = std::chrono::duration<float>(m_clock.stepDuration()).count();
    telemetry::TickTelemetry tick = m_host.captureTelemetry(stepIndex, stepSeconds);

    if (m_snapshotCallback) {
        m_snapshotCallback(tick);
    }

    m_telemetry.push(std::move(tick));
    reportTelemetry(stepIndex);
}

void Engine::reportTelemetry(std::uint64_t stepIndex) {
    if (stepIndex - m_lastTelemetryReportStep < kTelemetryReportInterval) return;
    m_lastTelemetryReportStep = stepIndex;
    const auto& entries = m_telemetry.entries();
    if (entries.empty()) return;
    const auto& latest = entries.back();
    const std::uint32_t agentCount = static_cast<std::uint32_t>(latest.agents.size());
    spdlog::info("Telemetry step {}: agents={}", stepIndex, agentCount);
}

const genesis::telemetry::TickTelemetry* Engine::latestTelemetry() const noexcept {
    const auto& entries = m_telemetry.entries();
    if (entries.empty()) return nullptr;
    return &entries.back();
}

std::uint32_t Engine::createAgent(const simulation::AgentSpawnParams2D& params) {
    return m_host.createAgent(params);
}

std::uint32_t Engine::createAgent2D(const genesis::agents::components::AgentLocation2D& location,
                                    const std::optional<genesis::agents::components::MovementIntent2D>& intent) {
    simulation::AgentSpawnParams2D params{};
    params.location.mapId = location.mapId;
    params.location.x = location.x;
    params.location.y = location.y;
    if (intent) {
        simulation::MovementCommand2D command{};
        command.targetMapId = intent->targetMapId;
        command.targetX = intent->targetX;
        command.targetY = intent->targetY;
        command.speed = intent->speed;
        params.initialMovement = command;
    }
    return createAgent(params);
}

bool Engine::setAgentMovementIntent(std::uint32_t entityId, const genesis::agents::components::MovementIntent2D& intent) {
    simulation::MovementCommand2D command{};
    command.targetMapId = intent.targetMapId;
    command.targetX = intent.targetX;
    command.targetY = intent.targetY;
    command.speed = intent.speed;
    return setAgentMovementIntent(entityId, command);
}

bool Engine::setAgentMovementIntent(std::uint32_t entityId, const simulation::MovementCommand2D& command) {
    return m_host.setAgentMovementIntent(entityId, command);
}

bool Engine::clearAgentMovementIntent(std::uint32_t entityId) {
    return m_host.clearAgentMovementIntent(entityId);
}

bool Engine::teleportAgent(std::uint32_t entityId, const genesis::agents::components::AgentLocation2D& target) {
    simulation::AgentPose2D pose{};
    pose.mapId = target.mapId;
    pose.x = target.x;
    pose.y = target.y;
    return teleportAgent(entityId, pose);
}

bool Engine::teleportAgent(std::uint32_t entityId, const simulation::AgentPose2D& target) {
    return m_host.teleportAgent(entityId, target);
}

bool Engine::deleteAgent(std::uint32_t entityId) {
    return m_host.deleteAgent(entityId);
}

bool Engine::agentExists(std::uint32_t entityId) const {
    return m_host.agentExists(entityId);
}

std::optional<simulation::AgentPose2D> Engine::queryAgentPose(std::uint32_t entityId) const {
    return m_host.queryAgentPose(entityId);
}

std::optional<genesis::agents::components::AgentLocation2D> Engine::queryAgentLocation(std::uint32_t entityId) const {
    auto pose = queryAgentPose(entityId);
    if (!pose) {
        return std::nullopt;
    }
    genesis::agents::components::AgentLocation2D loc{};
    loc.mapId = pose->mapId;
    loc.x = pose->x;
    loc.y = pose->y;
    return loc;
}

std::uint32_t Engine::consumeResource(std::uint32_t interactionId, std::uint32_t amount) {
    return m_host.consumeResource(interactionId, amount);
}

} // namespace genesis::core

