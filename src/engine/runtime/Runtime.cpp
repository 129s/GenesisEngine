#include "genesis/runtime/Runtime.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "EngineSimulationService.hpp"

#include "genesis/world/WorldDatabase.hpp"
#include "genesis/world/WorldDatabaseLoader.hpp"
#include "genesis/simulation/Namespace.hpp"
#include "genesis/world/WorldDatabaseSaver.hpp"
#include "genesis/worldgen/ConfigLoader.hpp"
#include "genesis/worldgen/Generator.hpp"

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

struct CoordNormalization {
    std::unordered_map<std::size_t, std::pair<int, int>> coordsByLocalId;
};

CoordNormalization normalizeNodePlacements(const genesis::worldgen::LayoutDraft& layout, int baseExtent) {
    CoordNormalization result{};
    if (baseExtent < 4) {
        baseExtent = 4;
    }

    if (layout.placements.empty()) {
        return result;
    }

    double minX = layout.placements.front().x;
    double maxX = layout.placements.front().x;
    double minY = layout.placements.front().y;
    double maxY = layout.placements.front().y;
    for (const auto& p : layout.placements) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }

    const double spanX = std::max(1e-6, maxX - minX);
    const double spanY = std::max(1e-6, maxY - minY);

    constexpr int padding = 2;
    const int minCoord = padding;
    const int maxCoord = std::max(minCoord, baseExtent - 1 - padding);

    result.coordsByLocalId.reserve(layout.placements.size());
    for (const auto& p : layout.placements) {
        const double ux = (p.x - minX) / spanX;
        const double uy = (p.y - minY) / spanY;
        const int x = static_cast<int>(std::lround(minCoord + ux * (maxCoord - minCoord)));
        const int y = static_cast<int>(std::lround(minCoord + uy * (maxCoord - minCoord)));
        result.coordsByLocalId.emplace(p.local_id, std::make_pair(x, y));
    }
    return result;
}

world::InMemoryWorldDatabase buildWorldDbFromDrafts(const genesis::worldgen::GeneratorConfig& config,
                                                    const genesis::worldgen::TopologyDraft& topology,
                                                    const genesis::worldgen::LayoutDraft& layout) {
    world::InMemoryWorldDatabase db;
    db.clear();

    const int extent = std::max(4, config.tilemap.base_extent);
    const auto normalized = normalizeNodePlacements(layout, extent);

    auto coordFor = [&](std::size_t localId) -> std::pair<int, int> {
        if (auto it = normalized.coordsByLocalId.find(localId); it != normalized.coordsByLocalId.end()) {
            return it->second;
        }
        const int mid = std::max(0, config.tilemap.base_extent / 2);
        return {mid, mid};
    };

    for (const auto& node : topology.nodes) {
        const auto mapId = static_cast<world::MapId>(node.local_id + 1);
        world::Map m{};
        m.id = mapId;
        m.name = node.label.empty() ? ("Map_" + std::to_string(mapId)) : (node.label + "_" + std::to_string(mapId));
        db.addMap(std::move(m));

        world::Scene s{};
        s.id = static_cast<world::SceneId>(mapId * 100U);
        s.mapId = mapId;
        s.name = node.label.empty() ? ("Scene_" + std::to_string(s.id)) : (node.label + "_scene");
        db.addScene(std::move(s));

        const auto baseCoord = coordFor(node.local_id);

        const auto resourcesPerMap = std::max<std::size_t>(1, config.worlddb.resources.per_map);
        constexpr world::InteractionId portalIdOffset = 900U;
        const auto resourceIdBase = static_cast<world::InteractionId>(mapId * 1000U);
        for (std::size_t index = 0; index < resourcesPerMap; ++index) {
            const int dx = static_cast<int>((index % 3) - 1);
            const int dy = static_cast<int>((index / 3) - 1);

            world::Interaction resource{};
            resource.id = static_cast<world::InteractionId>(resourceIdBase + static_cast<world::InteractionId>(index));
            if (resource.id >= resourceIdBase + portalIdOffset) {
                break;
            }
            resource.mapId = mapId;
            resource.sceneId = static_cast<world::SceneId>(mapId * 100U);
            resource.kind = world::InteractionKind::Resource;
            resource.coord = {std::clamp(baseCoord.first + 1 + dx, 0, extent - 1),
                              std::clamp(baseCoord.second + 1 + dy, 0, extent - 1)};
            if (resourcesPerMap == 1) {
                resource.name = "Resource";
            } else {
                resource.name = "Resource_" + std::to_string(index);
            }
            resource.capacity = config.worlddb.resources.capacity;
            resource.regenPerStep = config.worlddb.resources.regen_per_step;
            db.addInteraction(std::move(resource));
        }
    }

    {
        std::set<std::tuple<world::MapId, world::MapId, bool>> seen;
        for (const auto& e : topology.edges) {
            const auto from = static_cast<world::MapId>(e.from + 1);
            const auto to = static_cast<world::MapId>(e.to + 1);
            if (from == 0U || to == 0U || from == to) {
                continue;
            }
            const auto key = std::make_tuple(from, to, e.bidirectional);
            if (!seen.emplace(key).second) {
                continue;
            }
            db.addMapEdge(world::MapEdge{from, to, e.bidirectional});
        }
    }

    for (std::size_t index = 0; index < topology.portals.size(); ++index) {
        const auto& portal = topology.portals[index];
        const auto entryMap = static_cast<world::MapId>(portal.entry + 1);
        const auto exitMap = static_cast<world::MapId>(portal.exit + 1);
        if (entryMap == 0U || exitMap == 0U || entryMap == exitMap) {
            continue;
        }

        const auto entryCoord = coordFor(portal.entry);
        const auto exitCoord = coordFor(portal.exit);

        world::Interaction portalInter{};
        portalInter.id = static_cast<world::InteractionId>(entryMap * 1000U + 900U + static_cast<world::InteractionId>(index));
        portalInter.mapId = entryMap;
        portalInter.sceneId = static_cast<world::SceneId>(entryMap * 100U);
        portalInter.kind = world::InteractionKind::Portal;
        portalInter.coord = entryCoord;
        portalInter.name = "PortalTo_" + std::to_string(exitMap);
        db.addInteraction(portalInter);

        world::Portal p{};
        p.interactionId = portalInter.id;
        p.targetMapId = exitMap;
        p.targetSceneId = static_cast<world::SceneId>(exitMap * 100U);
        p.targetCoord = exitCoord;
        db.addPortal(std::move(p));
    }

    return db;
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
                                                                      std::optional<std::uint64_t> seedOverride,
                                                                      std::optional<std::filesystem::path> outputPath) {
    WorldGenerationResult result{};
    result.configPath = std::filesystem::absolute(configPath);

    const auto start = std::chrono::steady_clock::now();

    try {
        std::uint64_t seedValue = 0;
        if (seedOverride) {
            seedValue = *seedOverride;
        } else {
            seedValue = std::random_device{}();
        }
        result.seed.value = seedValue;
        m_lastSeed = seedValue;

        auto config = genesis::worldgen::load_config(result.configPath);

        std::filesystem::path folder;
        if (outputPath) {
            folder = std::filesystem::absolute(*outputPath);
        } else {
            folder = result.configPath.parent_path() / "worldgen_output" / ("world_" + std::to_string(seedValue));
            folder = std::filesystem::absolute(folder);
        }
        result.outputPath = folder;

        const auto generated = genesis::worldgen::generate_world(config, genesis::worldgen::Seed{seedValue});

        result.locationCount = generated.location_count;
        result.edgeCount = generated.edge_count;
        result.logs.clear();
        result.logs.reserve(generated.logs.size());
        for (const auto& entry : generated.logs) {
            result.logs.push_back(entry.message);
        }

        auto db = buildWorldDbFromDrafts(config, generated.topology, generated.layout);
        auto save = world::saveWorldDatabaseToFolder(folder, db);
        if (!save.success) {
            result.success = false;
            result.error = save.error.empty() ? "world database save failed" : save.error;
        } else {
            result.success = true;
        }
    } catch (const std::exception& ex) {
        result.success = false;
        result.error = ex.what();
    } catch (...) {
        result.success = false;
        result.error = "world generation failed (unknown error)";
    }

    const auto end = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();

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
