#include "genesis/runtime/Runtime.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "EngineSimulationService.hpp"

#include "genesis/world/WorldDatabaseLoader.hpp"
#include "genesis/simulation/Namespace.hpp"
#include "genesis/world/WorldDatabaseSaver.hpp"

namespace Genesis::Runtime {

namespace simulation = Genesis::Simulation;
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

class Runtime::Impl {
public:
    explicit Impl(RuntimeConfig config);

    void step(Runtime& owner, std::uint64_t steps);
    void run(Runtime& owner, std::uint64_t steps);

    [[nodiscard]] const SimulationSnapshot* latestSnapshot() const noexcept;
    [[nodiscard]] std::optional<SimulationSnapshotDiff> latestSnapshotDiff() const noexcept;

    [[nodiscard]] const std::optional<std::uint64_t>& lastSeed() const noexcept { return m_lastSeed; }
    [[nodiscard]] const std::optional<WorldGenerationResult>& lastWorldGeneration() const noexcept { return m_lastWorldGen; }

    WorldGenerationResult generateWorldFromConfig(const std::filesystem::path& configPath,
                                                  std::optional<std::uint64_t> seedOverride,
                                                  std::optional<std::filesystem::path> outputPath);

    world::WorldDbLoadResult loadWorldFromFile(const std::filesystem::path& path);
    world::WorldDbSaveResult saveWorldToFile(const std::filesystem::path& folder) const;

    [[nodiscard]] std::shared_ptr<world::WorldDatabase> worldDatabase() const noexcept;

    std::uint64_t enqueueEvent(RuntimeEvent event);

    std::uint32_t createAgent(const simulation::AgentSpawnParams2D& params);
    bool setAgentMovementIntent(std::uint32_t entityId, const simulation::MovementCommand2D& command);
    bool stopAgentMovement(std::uint32_t entityId);
    bool teleportAgent(std::uint32_t entityId, const simulation::AgentPose2D& target);
    bool deleteAgent(std::uint32_t entityId);
    bool agentExists(std::uint32_t entityId) const;
    std::optional<simulation::AgentPose2D> agentPose(std::uint32_t entityId) const;
    std::uint32_t consumeResource(std::uint32_t interactionId, std::uint32_t amount);

private:
    void drainPendingEvents(Runtime& owner);
    void onSimulationSnapshot(const telemetry::TickTelemetry& tick);

    struct EventState {
        void enqueue(RuntimeEvent event);
        std::queue<RuntimeEvent> takePending();
        void appendExecuted(std::vector<RuntimeEventReport>&& reports);
        std::vector<RuntimeEventReport> takeExecutedSinceSnapshot();

    private:
        std::mutex mutex;
        std::queue<RuntimeEvent> pending;
        std::vector<RuntimeEventReport> executed;
    };

    RuntimeConfig m_config;
    std::unique_ptr<SimulationService> m_simulation;
    std::optional<std::uint64_t> m_lastSeed;
    std::optional<WorldGenerationResult> m_lastWorldGen;
    std::atomic<std::uint64_t> m_snapshotVersion{0};
    SimulationSnapshotBuffer m_snapshotBuffer;
    std::atomic<std::uint64_t> m_nextEventId{1};
    EventState m_events;
};

Runtime::Impl::Impl(RuntimeConfig config)
    : m_config(std::move(config)) {
    if (m_config.simulationFactory) {
        m_simulation = m_config.simulationFactory();
    }
    if (!m_simulation) {
        m_simulation = std::make_unique<EngineSimulationService>();
    }

    m_simulation->setSnapshotCallback([this](const telemetry::TickTelemetry& tick) {
        onSimulationSnapshot(tick);
    });

    try {
        if (!spdlog::default_logger()) {
            auto logger = spdlog::stdout_color_mt("genesis");
            spdlog::set_default_logger(std::move(logger));
            spdlog::set_level(spdlog::level::info);
        }
    } catch (...) {
        // 日志初始化失败不影响运行时
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

void Runtime::Impl::step(Runtime& owner, std::uint64_t steps) {
    for (std::uint64_t processed = 0; processed < steps; ++processed) {
        drainPendingEvents(owner);
        m_simulation->step(1);
    }
}

void Runtime::Impl::run(Runtime& owner, std::uint64_t steps) {
    for (std::uint64_t processed = 0; processed < steps; ++processed) {
        drainPendingEvents(owner);
        m_simulation->run(1);
    }
}

const SimulationSnapshot* Runtime::Impl::latestSnapshot() const noexcept {
    return m_snapshotBuffer.latest();
}

std::optional<SimulationSnapshotDiff> Runtime::Impl::latestSnapshotDiff() const noexcept {
    const auto view = m_snapshotBuffer.latestPair();
    if (!view) {
        return std::nullopt;
    }
    return diffSnapshots(view->previous, *view->latest);
}

Runtime::WorldGenerationResult Runtime::Impl::generateWorldFromConfig(const std::filesystem::path& configPath,
                                                                      std::optional<std::uint64_t>,
                                                                      std::optional<std::filesystem::path>) {
    WorldGenerationResult result{};
    result.configPath = std::filesystem::absolute(configPath);
    result.success = false;
    result.error = "world generation is not supported in v2 runtime";
    m_lastWorldGen = result;
    return *m_lastWorldGen;
}

world::WorldDbLoadResult Runtime::Impl::loadWorldFromFile(const std::filesystem::path& path) {
    const auto absolute = std::filesystem::absolute(path);
    auto result = m_simulation->loadWorld(absolute);
    if (result.success) {
        spdlog::info("Runtime loaded world DB from {}", absolute.string());
    }
    return result;
}

world::WorldDbSaveResult Runtime::Impl::saveWorldToFile(const std::filesystem::path& folder) const {
    world::WorldDbSaveResult r{};
    auto db = worldDatabase();
    if (!db) {
        r.success = false;
        r.error = "no world database loaded";
        return r;
    }
    return world::saveWorldDatabaseToFolder(folder, *db);
}

std::shared_ptr<world::WorldDatabase> Runtime::Impl::worldDatabase() const noexcept {
    return m_simulation ? m_simulation->worldDatabase() : nullptr;
}

std::uint64_t Runtime::Impl::enqueueEvent(RuntimeEvent event) {
    const auto id = m_nextEventId.fetch_add(1, std::memory_order_relaxed);
    event.id = id;
    event.enqueuedAt = std::chrono::steady_clock::now();
    m_events.enqueue(std::move(event));
    return id;
}

std::uint32_t Runtime::Impl::createAgent(const simulation::AgentSpawnParams2D& params) {
    return m_simulation->createAgent(params);
}

bool Runtime::Impl::setAgentMovementIntent(std::uint32_t entityId, const simulation::MovementCommand2D& command) {
    return m_simulation->setAgentMovementIntent(entityId, command);
}

bool Runtime::Impl::stopAgentMovement(std::uint32_t entityId) {
    return m_simulation->clearAgentMovementIntent(entityId);
}

bool Runtime::Impl::teleportAgent(std::uint32_t entityId, const simulation::AgentPose2D& target) {
    return m_simulation->teleportAgent(entityId, target);
}

bool Runtime::Impl::deleteAgent(std::uint32_t entityId) {
    return m_simulation->deleteAgent(entityId);
}

bool Runtime::Impl::agentExists(std::uint32_t entityId) const {
    return m_simulation->agentExists(entityId);
}

std::optional<simulation::AgentPose2D> Runtime::Impl::agentPose(std::uint32_t entityId) const {
    return m_simulation->queryAgentPose(entityId);
}

std::uint32_t Runtime::Impl::consumeResource(std::uint32_t interactionId, std::uint32_t amount) {
    return m_simulation->consumeResource(interactionId, amount);
}

void Runtime::Impl::drainPendingEvents(Runtime& owner) {
    auto local = m_events.takePending();
    if (local.empty()) {
        return;
    }

    std::vector<RuntimeEventReport> executed;
    executed.reserve(static_cast<std::size_t>(local.size()));

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
                event.runtimeHandler(owner);
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

    m_events.appendExecuted(std::move(executed));
}

void Runtime::Impl::onSimulationSnapshot(const telemetry::TickTelemetry& tick) {
    SimulationSnapshot snapshot{};
    snapshot.version = m_snapshotVersion.fetch_add(1, std::memory_order_relaxed) + 1;
    snapshot.capturedAt = std::chrono::steady_clock::now();
    snapshot.telemetry = tick;
    auto events = m_events.takeExecutedSinceSnapshot();
    if (!events.empty()) {
        snapshot.events = std::move(events);
    }
    m_snapshotBuffer.write(std::move(snapshot));
}

void Runtime::Impl::EventState::enqueue(RuntimeEvent event) {
    std::scoped_lock lock(mutex);
    pending.push(std::move(event));
}

std::queue<RuntimeEvent> Runtime::Impl::EventState::takePending() {
    std::scoped_lock lock(mutex);
    std::queue<RuntimeEvent> local;
    std::swap(local, pending);
    return local;
}

void Runtime::Impl::EventState::appendExecuted(std::vector<RuntimeEventReport>&& reports) {
    if (reports.empty()) {
        return;
    }
    std::scoped_lock lock(mutex);
    if (executed.empty()) {
        executed = std::move(reports);
    } else {
        executed.reserve(executed.size() + reports.size());
        std::move(reports.begin(), reports.end(), std::back_inserter(executed));
    }
}

std::vector<RuntimeEventReport> Runtime::Impl::EventState::takeExecutedSinceSnapshot() {
    std::scoped_lock lock(mutex);
    std::vector<RuntimeEventReport> out = std::move(executed);
    executed.clear();
    return out;
}

Runtime::Runtime(RuntimeConfig config)
    : m_impl(std::make_unique<Impl>(std::move(config))) {}

Runtime::~Runtime() = default;

void Runtime::step(std::uint64_t steps) {
    m_impl->step(*this, steps);
}

void Runtime::run(std::uint64_t steps) {
    m_impl->run(*this, steps);
}

const SimulationSnapshot* Runtime::latestSnapshot() const noexcept {
    return m_impl->latestSnapshot();
}

std::optional<SimulationSnapshotDiff> Runtime::latestSnapshotDiff() const noexcept {
    return m_impl->latestSnapshotDiff();
}

const std::optional<std::uint64_t>& Runtime::lastSeed() const noexcept {
    return m_impl->lastSeed();
}

Runtime::WorldGenerationResult Runtime::generateWorldFromConfig(const std::filesystem::path& configPath,
                                                                std::optional<std::uint64_t> seedOverride,
                                                                std::optional<std::filesystem::path> outputPath) {
    return m_impl->generateWorldFromConfig(configPath, seedOverride, outputPath);
}

const std::optional<Runtime::WorldGenerationResult>& Runtime::lastWorldGeneration() const noexcept {
    return m_impl->lastWorldGeneration();
}

world::WorldDbLoadResult Runtime::loadWorldFromFile(const std::filesystem::path& path) {
    return m_impl->loadWorldFromFile(path);
}

world::WorldDbSaveResult Runtime::saveWorldToFile(const std::filesystem::path& path) const {
    return m_impl->saveWorldToFile(path);
}

std::shared_ptr<world::WorldDatabase> Runtime::worldDatabase() const noexcept {
    return m_impl->worldDatabase();
}

std::uint64_t Runtime::enqueueEvent(RuntimeEvent event) {
    return m_impl->enqueueEvent(std::move(event));
}

std::uint32_t Runtime::createAgent(const simulation::AgentSpawnParams2D& params) {
    return m_impl->createAgent(params);
}

std::uint32_t Runtime::createAgent2D(const agent_components::AgentLocation2D& location,
                                     const std::optional<agent_components::MovementIntent2D>& intent) {
    return createAgent(toSpawnParams(location, intent));
}

bool Runtime::setAgentMovementIntent(std::uint32_t entityId, const simulation::MovementCommand2D& command) {
    return m_impl->setAgentMovementIntent(entityId, command);
}

bool Runtime::setAgentMovementIntent(std::uint32_t entityId, const agent_components::MovementIntent2D& intent) {
    return setAgentMovementIntent(entityId, toMovementCommand(intent));
}

bool Runtime::stopAgentMovement(std::uint32_t entityId) {
    return m_impl->stopAgentMovement(entityId);
}

bool Runtime::teleportAgent(std::uint32_t entityId, const simulation::AgentPose2D& target) {
    return m_impl->teleportAgent(entityId, target);
}

bool Runtime::teleportAgent(std::uint32_t entityId, const agent_components::AgentLocation2D& target) {
    return teleportAgent(entityId, toPose(target));
}

bool Runtime::deleteAgent(std::uint32_t entityId) {
    return m_impl->deleteAgent(entityId);
}

std::uint32_t Runtime::consumeResource(std::uint32_t interactionId, std::uint32_t amount) {
    return m_impl->consumeResource(interactionId, amount);
}

bool Runtime::agentExists(std::uint32_t entityId) const {
    return m_impl->agentExists(entityId);
}

std::optional<simulation::AgentPose2D> Runtime::agentPose(std::uint32_t entityId) const {
    return m_impl->agentPose(entityId);
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

std::unique_ptr<Runtime> createRuntime(RuntimeConfig config) {
    return std::make_unique<Runtime>(std::move(config));
}

} // namespace Genesis::Runtime
