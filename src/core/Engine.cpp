#include "genesis/core/Engine.hpp"
#include "genesis/world/WorldDatabaseLoader.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <spdlog/spdlog.h>

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
    m_simulation.tick(m_registry, deltaSeconds, stepIndex);
    captureTelemetry(stepIndex);
}

genesis::world::WorldDbLoadResult Engine::loadWorldFromFile(const std::filesystem::path& folder) {
    auto res = genesis::world::loadWorldDatabaseFromFolder(folder);
    if (res.success && res.database) {
        destroyAllAgents();
        m_worldDb = res.database;
        initializeResourcesFromDatabase();
        spawnDemoAgentsIfEmpty();
    }
    return res;
}

void Engine::captureTelemetry(std::uint64_t stepIndex) {
    m_simulation.collectResourceSnapshots(m_registry, m_resourceScratch);
    telemetry::TickTelemetry tick = m_telemetryCollector.collect(
        m_registry,
        m_worldDb ? m_worldDb.get() : nullptr,
        m_resourceScratch,
        stepIndex,
        std::chrono::duration<float>(m_clock.stepDuration()).count());

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

void Engine::spawnDemoAgentsIfEmpty() {
    if (m_registry.view<genesis::agents::components::AgentLocation2D>().empty()) {
        genesis::agents::components::AgentLocation2D loc{};
        loc.mapId = 1;
        loc.x = 0.0f;
        loc.y = 0.0f;
        genesis::agents::components::MovementIntent2D intent{};
        intent.targetMapId = 1;
        intent.targetX = 10.0f;
        intent.targetY = 5.0f;
        intent.speed = 2.0f;
        (void)createAgent2D(loc, std::optional{intent});
    }
}

void Engine::initializeResourcesFromDatabase() {
    m_simulation.setWorldDatabase(m_worldDb, m_registry);
}

std::uint32_t Engine::createAgent2D(const genesis::agents::components::AgentLocation2D& location,
                                    const std::optional<genesis::agents::components::MovementIntent2D>& intent) {
    auto entity = m_registry.create();
    m_registry.emplace<genesis::agents::components::AgentLocation2D>(entity, location);
    if (intent) {
        m_registry.emplace<genesis::agents::components::MovementIntent2D>(entity, *intent);
    }
    return static_cast<std::uint32_t>(entt::to_integral(entity));
}

bool Engine::setAgentMovementIntent(std::uint32_t entityId, const genesis::agents::components::MovementIntent2D& intent) {
    const auto entity = toEntity(entityId);
    if (!m_registry.valid(entity) || !m_registry.all_of<genesis::agents::components::AgentLocation2D>(entity)) {
        return false;
    }
    if (auto* existing = m_registry.try_get<genesis::agents::components::MovementIntent2D>(entity)) {
        *existing = intent;
    } else {
        m_registry.emplace<genesis::agents::components::MovementIntent2D>(entity, intent);
    }
    return true;
}

bool Engine::clearAgentMovementIntent(std::uint32_t entityId) {
    const auto entity = toEntity(entityId);
    if (!m_registry.valid(entity)) {
        return false;
    }
    if (m_registry.any_of<genesis::agents::components::MovementIntent2D>(entity)) {
        m_registry.remove<genesis::agents::components::MovementIntent2D>(entity);
    }
    return true;
}

bool Engine::teleportAgent(std::uint32_t entityId, const genesis::agents::components::AgentLocation2D& target) {
    const auto entity = toEntity(entityId);
    if (!m_registry.valid(entity)) {
        return false;
    }
    auto& loc = m_registry.get_or_emplace<genesis::agents::components::AgentLocation2D>(entity);
    loc = target;
    if (m_registry.any_of<genesis::agents::components::MovementIntent2D>(entity)) {
        m_registry.remove<genesis::agents::components::MovementIntent2D>(entity);
    }
    return true;
}

bool Engine::deleteAgent(std::uint32_t entityId) {
    const auto entity = toEntity(entityId);
    if (!m_registry.valid(entity)) {
        return false;
    }
    m_registry.destroy(entity);
    return true;
}

bool Engine::agentExists(std::uint32_t entityId) const {
    const auto entity = toEntity(entityId);
    return m_registry.valid(entity) && m_registry.all_of<genesis::agents::components::AgentLocation2D>(entity);
}

std::optional<genesis::agents::components::AgentLocation2D> Engine::queryAgentLocation(std::uint32_t entityId) const {
    const auto entity = toEntity(entityId);
    if (!m_registry.valid(entity) || !m_registry.all_of<genesis::agents::components::AgentLocation2D>(entity)) {
        return std::nullopt;
    }
    return m_registry.get<genesis::agents::components::AgentLocation2D>(entity);
}

std::uint32_t Engine::consumeResource(std::uint32_t interactionId, std::uint32_t amount) {
    auto* resourceSystem = m_simulation.resourceSystem();
    if (!resourceSystem) {
        return 0;
    }
    return resourceSystem->consume(m_registry, genesis::world::ResourceType::Food, amount, interactionId);
}

entt::entity Engine::toEntity(std::uint32_t id) const noexcept {
    return static_cast<entt::entity>(id);
}

void Engine::destroyAllAgents() {
    m_simulation.reset(m_registry);
    m_registry.clear();
}

} // namespace genesis::core

