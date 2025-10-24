// New minimal Engine implementation (v2)
#include "genesis/core/Engine.hpp"
#include "genesis/world/WorldDatabaseLoader.hpp"
#include "genesis/world/WorldLoader.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
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
    m_scheduler.update(m_registry, deltaSeconds);
    captureTelemetry(stepIndex);
}

genesis::world::WorldLoadResult Engine::loadWorldFromFile(const std::filesystem::path& folder) {
    genesis::world::WorldLoadResult out{};
    auto res = genesis::world::loadWorldDatabaseFromFolder(folder);
    if (!res.success || !res.database) {
        out.success = false;
        out.error = res.error.empty() ? std::string("未能加载世界数据库") : res.error;
        return out;
    }
    m_worldDb = std::move(res.database);
    spawnDemoAgentsIfEmpty();
    out.success = true;
    return out;
}

void Engine::captureTelemetry(std::uint64_t stepIndex) {
    telemetry::TickTelemetry tick = m_telemetryCollector.collect(m_registry, stepIndex, std::chrono::duration<float>(m_clock.stepDuration()).count());

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
        auto e = m_registry.create();
        genesis::agents::components::AgentLocation2D loc{};
        loc.mapId = 1; loc.x = 0.0f; loc.y = 0.0f;
        m_registry.emplace<genesis::agents::components::AgentLocation2D>(e, loc);
        genesis::agents::components::MovementIntent2D intent{};
        intent.targetMapId = 1; intent.targetX = 10.0f; intent.targetY = 5.0f; intent.speed = 2.0f;
        m_registry.emplace<genesis::agents::components::MovementIntent2D>(e, intent);
    }
}

} // namespace genesis::core

