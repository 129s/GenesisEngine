#include "genesis/runtime/Runtime.hpp"

#include <chrono>
#include <exception>
#include <filesystem>
#include <iterator>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "EngineSimulationService.hpp"

#include "genesis/world/WorldDatabaseLoader.hpp"
#include "genesis/world/WorldDatabaseSaver.hpp"

namespace Genesis::Runtime {

namespace simulation = genesis::simulation;
namespace telemetry = genesis::telemetry;
namespace world = Genesis::World;
namespace agents = Genesis::Agents;
namespace agent_components = Genesis::Agents::Components;


namespace {

simulation::AgentSpawnParams2D toSpawnParams(const agent_components::AgentLocation2D& location,
                                            const std::optional<agent_components::MovementIntent2D>& intent) {
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
    return params;
}

simulation::MovementCommand2D toMovementCommand(const agent_components::MovementIntent2D& intent) {
    simulation::MovementCommand2D command{};
    command.targetMapId = intent.targetMapId;
    command.targetX = intent.targetX;
    command.targetY = intent.targetY;
    command.speed = intent.speed;
    return command;
}

simulation::AgentPose2D toPose(const agent_components::AgentLocation2D& target) {
    simulation::AgentPose2D pose{};
    pose.mapId = target.mapId;
    pose.x = target.x;
    pose.y = target.y;
    return pose;
}

} // namespace


Runtime::Runtime(RuntimeConfig config)
    : m_config(std::move(config)) {
    if (m_config.simulationFactory) {
        m_simulation = m_config.simulationFactory();
    }
    if (!m_simulation) {
        m_simulation = std::make_unique<EngineSimulationService>();
    }

    m_simulation->setSnapshotCallback([this](const telemetry::TickTelemetry& tick) {
        SimulationSnapshot snapshot{};
        snapshot.version = m_snapshotVersion.fetch_add(1, std::memory_order_relaxed) + 1;
        snapshot.capturedAt = std::chrono::steady_clock::now();
        snapshot.telemetry = tick;
        {
            std::scoped_lock lock(m_eventMutex);
            if (!m_eventsSinceLastSnapshot.empty()) {
                snapshot.events = std::move(m_eventsSinceLastSnapshot);
                m_eventsSinceLastSnapshot.clear();
            }
        }
        m_snapshotBuffer.write(std::move(snapshot));
    });

    try {
        if (!spdlog::default_logger()) {
            auto logger = spdlog::stdout_color_mt("genesis");
            spdlog::set_default_logger(std::move(logger));
            spdlog::set_level(spdlog::level::info);
        }
    } catch (...) {
        // Logging initialization is best-effort in runtime context
    }

    if (m_config.initialWorldPath) {
        const auto absolute = std::filesystem::absolute(*m_config.initialWorldPath);
        const auto loadResult = loadWorldFromFile(absolute);
        if (!loadResult.success) {
            spdlog::error("Failed to load initial world {}: {}", absolute.string(), loadResult.error);
        }
    }

    if (m_config.bootstrapSteps > 0) {
        m_simulation->step(m_config.bootstrapSteps);
    }
}

void Runtime::step(std::uint64_t steps) {
    for (std::uint64_t processed = 0; processed < steps; ++processed) {
        drainPendingEvents();
        m_simulation->step(1);
    }
}

void Runtime::run(std::uint64_t steps) {
    for (std::uint64_t processed = 0; processed < steps; ++processed) {
        drainPendingEvents();
        m_simulation->run(1);
    }
}

const SimulationSnapshot* Runtime::latestSnapshot() const noexcept {
    return m_snapshotBuffer.latest();
}

std::optional<SimulationSnapshotDiff> Runtime::latestSnapshotDiff() const noexcept {
    const auto view = m_snapshotBuffer.latestPair();
    if (!view) {
        return std::nullopt;
    }
    return diffSnapshots(view->previous, *view->latest);
}

std::uint64_t Runtime::enqueueEvent(RuntimeEvent event) {
    event.id = m_nextEventId.fetch_add(1, std::memory_order_relaxed);
    const auto id = event.id;
    event.enqueuedAt = std::chrono::steady_clock::now();
    std::scoped_lock lock(m_eventMutex);
    m_pendingEvents.push(std::move(event));
    return id;
}

Runtime::WorldGenerationResult Runtime::generateWorldFromConfig(const std::filesystem::path& configPath, std::optional<std::uint64_t>, std::optional<std::filesystem::path>) {
    WorldGenerationResult result{};
    result.configPath = std::filesystem::absolute(configPath);
    result.success = false;
    result.error = "world generation is not supported in v2 runtime";
    m_lastWorldGen = result;
    return *m_lastWorldGen;
}

world::WorldDbLoadResult Runtime::loadWorldFromFile(const std::filesystem::path& path) {
    const auto absolute = std::filesystem::absolute(path);
    auto result = m_simulation->loadWorld(absolute);
    if (result.success) {
        spdlog::info("Runtime loaded world DB from {}", absolute.string());
    }
    return result;
}

world::WorldDbSaveResult Runtime::saveWorldToFile(const std::filesystem::path& folder) const {
    world::WorldDbSaveResult r{};
    auto db = worldDatabase();
    if (!db) {
        r.success = false;
        r.error = "no world database loaded";
        return r;
    }
    return world::saveWorldDatabaseToFolder(folder, *db);
}

std::unique_ptr<Runtime> createRuntime(RuntimeConfig config) {
    return std::make_unique<Runtime>(std::move(config));
}

std::shared_ptr<world::WorldDatabase> Runtime::worldDatabase() const noexcept {
    return m_simulation ? m_simulation->worldDatabase() : nullptr;
}

std::uint32_t Runtime::createAgent(const simulation::AgentSpawnParams2D& params) {
    return m_simulation->createAgent(params);
}

std::uint32_t Runtime::createAgent2D(const agent_components::AgentLocation2D& location,
                                     const std::optional<agent_components::MovementIntent2D>& intent) {
    return createAgent(toSpawnParams(location, intent));
}

bool Runtime::setAgentMovementIntent(std::uint32_t entityId, const simulation::MovementCommand2D& command) {
    return m_simulation->setAgentMovementIntent(entityId, command);
}

bool Runtime::setAgentMovementIntent(std::uint32_t entityId, const agent_components::MovementIntent2D& intent) {
    return setAgentMovementIntent(entityId, toMovementCommand(intent));
}

bool Runtime::stopAgentMovement(std::uint32_t entityId) {
    return m_simulation->clearAgentMovementIntent(entityId);
}

bool Runtime::teleportAgent(std::uint32_t entityId, const simulation::AgentPose2D& target) {
    return m_simulation->teleportAgent(entityId, target);
}

bool Runtime::teleportAgent(std::uint32_t entityId, const agent_components::AgentLocation2D& target) {
    return teleportAgent(entityId, toPose(target));
}

bool Runtime::deleteAgent(std::uint32_t entityId) {
    return m_simulation->deleteAgent(entityId);
}

std::uint32_t Runtime::consumeResource(std::uint32_t interactionId, std::uint32_t amount) {
    return m_simulation->consumeResource(interactionId, amount);
}

bool Runtime::agentExists(std::uint32_t entityId) const {
    return m_simulation->agentExists(entityId);
}

std::optional<simulation::AgentPose2D> Runtime::agentPose(std::uint32_t entityId) const {
    return m_simulation->queryAgentPose(entityId);
}

std::optional<agent_components::AgentLocation2D> Runtime::agentLocation(std::uint32_t entityId) const {
    auto pose = agentPose(entityId);
    if (!pose) {
        return std::nullopt;
    }
    agent_components::AgentLocation2D location{};
    location.mapId = pose->mapId;
    location.x = pose->x;
    location.y = pose->y;
    return location;
}

void Runtime::drainPendingEvents() {
    std::queue<RuntimeEvent> local;
    {
        std::scoped_lock lock(m_eventMutex);
        if (m_pendingEvents.empty()) {
            return;
        }
        std::swap(local, m_pendingEvents);
    }

    std::vector<RuntimeEventReport> executed;
    executed.reserve(local.size());

    while (!local.empty()) {
        auto event = std::move(local.front());
        local.pop();

        RuntimeEventReport report{};
        report.id = event.id;
        report.kind = event.kind;
        report.label = std::move(event.label);
        report.payloadJson = std::move(event.payloadJson);
        report.enqueuedAt = event.enqueuedAt;
        report.executedAt = std::chrono::steady_clock::now();

        try {
            if (event.runtimeHandler) {
                event.runtimeHandler(*this);
            } else if (event.simulationHandler) {
                if (!m_simulation) {
                    throw std::runtime_error("RuntimeEvent requires SimulationService, but runtime has no active simulation");
                }
                event.simulationHandler(*m_simulation);
            }
            report.success = true;
        } catch (const std::exception& ex) {
            report.success = false;
            report.message = ex.what();
        } catch (...) {
            report.success = false;
            report.message = "unknown error";
        }

        if (event.onComplete) {
            try {
                event.onComplete(report);
            } catch (const std::exception& ex) {
                spdlog::warn("RuntimeEvent onComplete failed: {}", ex.what());
            } catch (...) {
                spdlog::warn("RuntimeEvent onComplete failed with unknown error");
            }
        }

        executed.push_back(std::move(report));
    }

    if (!executed.empty()) {
        std::scoped_lock lock(m_eventMutex);
        m_eventsSinceLastSnapshot.insert(
            m_eventsSinceLastSnapshot.end(),
            std::make_move_iterator(executed.begin()),
            std::make_move_iterator(executed.end()));
    }
}

} // namespace Genesis::Runtime
