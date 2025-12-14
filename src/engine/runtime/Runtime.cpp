#include "genesis/runtime/Runtime.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

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

WorldAtlas buildWorldAtlasFromDatabase(const genesis::world::WorldDatabase& db, std::uint32_t worldVersion);

namespace {

using json = nlohmann::json;

[[nodiscard]] std::optional<std::string> getStringField(const json& obj, const char* key) {
    if (!obj.contains(key)) {
        return std::nullopt;
    }
    const auto& value = obj.at(key);
    if (!value.is_string()) {
        return std::nullopt;
    }
    return value.get<std::string>();
}

[[nodiscard]] std::optional<std::uint32_t> getU32Field(const json& obj, const char* key) {
    if (!obj.contains(key)) {
        return std::nullopt;
    }
    const auto& value = obj.at(key);
    if (!value.is_number_unsigned()) {
        if (value.is_number_integer()) {
            const auto v = value.get<std::int64_t>();
            if (v >= 0 && v <= static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max())) {
                return static_cast<std::uint32_t>(v);
            }
        }
        return std::nullopt;
    }
    return value.get<std::uint32_t>();
}

[[nodiscard]] std::optional<float> getFloatField(const json& obj, const char* key) {
    if (!obj.contains(key)) {
        return std::nullopt;
    }
    const auto& value = obj.at(key);
    if (!value.is_number()) {
        return std::nullopt;
    }
    return value.get<float>();
}

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

    std::unordered_set<std::size_t> sceneLocalIds;
    sceneLocalIds.reserve(topology.nodes.size());
    bool hasExplicitResources = false;
    bool hasInteractivePortals = false;
    for (const auto& node : topology.nodes) {
        if (node.kind == genesis::worldgen::DraftNodeKind::Scene) {
            sceneLocalIds.insert(node.local_id);
        } else if (node.kind == genesis::worldgen::DraftNodeKind::InteractiveResource) {
            hasExplicitResources = true;
        } else if (node.kind == genesis::worldgen::DraftNodeKind::InteractivePortal) {
            hasInteractivePortals = true;
        }
    }

    auto isSceneLocalId = [&](std::size_t localId) -> bool {
        return sceneLocalIds.find(localId) != sceneLocalIds.end();
    };

    std::unordered_map<world::MapId, world::InteractionId> nextInteractionId;
    nextInteractionId.reserve(topology.nodes.size());

    auto allocateInteractionId = [&](world::MapId mapId) -> std::optional<world::InteractionId> {
        constexpr world::InteractionId kPortalIdOffset = 900U;
        const auto base = static_cast<world::InteractionId>(mapId * 1000U);
        auto it = nextInteractionId.find(mapId);
        if (it == nextInteractionId.end()) {
            it = nextInteractionId.emplace(mapId, base).first;
        }
        if (it->second >= base + kPortalIdOffset) {
            return std::nullopt;
        }
        return it->second++;
    };

    std::unordered_map<world::MapId, std::pair<int, int>> mapCenters;
    mapCenters.reserve(topology.nodes.size());

    for (const auto& node : topology.nodes) {
        if (node.kind != genesis::worldgen::DraftNodeKind::Scene) {
            continue;
        }

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

        world::TilemapMeta tile{};
        tile.width = std::max(1, config.tilemap.base_extent);
        tile.height = std::max(1, config.tilemap.base_extent);
        tile.tileW = std::max(1, config.tilemap.tile_size);
        tile.tileH = std::max(1, config.tilemap.tile_size);
        db.setTilemap(mapId, std::move(tile));

        mapCenters.emplace(mapId, coordFor(node.local_id));
        nextInteractionId.emplace(mapId, static_cast<world::InteractionId>(mapId * 1000U));
    }

    if (!hasExplicitResources) {
        const auto selectConfigResourceType = [&](std::uint64_t ordinal) -> world::ResourceType {
            if (config.worlddb.resources.types.empty())
            {
                return config.worlddb.resources.type;
            }
            const auto& types = config.worlddb.resources.types;
            const auto& weights = config.worlddb.resources.weights;
            double sum = 0.0;
            if (weights.empty())
            {
                sum = static_cast<double>(types.size());
            }
            else
            {
                for (double w : weights) sum += w;
            }
            if (sum <= 0.0)
            {
                return types.front();
            }
            const double u = static_cast<double>((ordinal % 100000ULL) + 0.5) / 100000.0;
            const double r = u * sum;
            double acc = 0.0;
            for (std::size_t i = 0; i < types.size(); ++i)
            {
                acc += weights.empty() ? 1.0 : weights[i];
                if (r <= acc)
                {
                    return types[i];
                }
            }
            return types.back();
        };

        for (const auto& node : topology.nodes) {
            if (node.kind != genesis::worldgen::DraftNodeKind::Scene) {
                continue;
            }

            const auto mapId = static_cast<world::MapId>(node.local_id + 1);
            const auto sceneId = static_cast<world::SceneId>(mapId * 100U);
            const auto baseCoord = [&]() {
                if (auto it = mapCenters.find(mapId); it != mapCenters.end()) {
                    return it->second;
                }
                return coordFor(node.local_id);
            }();

            std::size_t resourcesPerMap = config.worlddb.resources.per_map;
            if (resourcesPerMap == 0) {
                continue;
            }
            if (std::find(node.tags.begin(), node.tags.end(), "hub") != node.tags.end()) {
                resourcesPerMap += 1;
            }
            if (std::find(node.tags.begin(), node.tags.end(), "corridor") != node.tags.end() && resourcesPerMap > 1) {
                resourcesPerMap -= 1;
            }

            for (std::size_t index = 0; index < resourcesPerMap; ++index) {
                const int dx = static_cast<int>((index % 3) - 1);
                const int dy = static_cast<int>((index / 3) - 1);

                auto idOpt = allocateInteractionId(mapId);
                if (!idOpt) {
                    break;
                }

                world::Interaction resource{};
                resource.id = *idOpt;
                resource.mapId = mapId;
                resource.sceneId = sceneId;
                resource.kind = world::InteractionKind::Resource;
                resource.resourceType = selectConfigResourceType((static_cast<std::uint64_t>(mapId) << 32ULL) ^ static_cast<std::uint64_t>(index));
                resource.coordLocal = {std::clamp(baseCoord.first + 1 + dx, 0, extent - 1),
                                       std::clamp(baseCoord.second + 1 + dy, 0, extent - 1)};
                resource.coordGlobal = resource.coordLocal;
                resource.name = (resourcesPerMap == 1) ? "Resource" : ("Resource_" + std::to_string(index));
                resource.capacity = config.worlddb.resources.capacity;
                resource.regenPerStep = config.worlddb.resources.regen_per_step;
                db.addInteraction(std::move(resource));
            }
        }
    }

    auto parseResourceTypeTag = [](const genesis::worldgen::NodeDraft& node) -> std::optional<world::ResourceType> {
        constexpr std::string_view prefix = "resource_type=";
        for (const auto& t : node.tags) {
            if (t.size() >= prefix.size() && t.compare(0, prefix.size(), prefix.data(), prefix.size()) == 0) {
                const auto value = t.substr(prefix.size());
                if (value == "Food" || value == "food") return world::ResourceType::Food;
                if (value == "Drink" || value == "drink" || value == "Water" || value == "water") return world::ResourceType::Drink;
                if (value == "Social" || value == "social") return world::ResourceType::Social;
                return std::nullopt;
            }
        }
        return std::nullopt;
    };

    for (const auto& node : topology.nodes) {
        if (node.kind != genesis::worldgen::DraftNodeKind::InteractiveResource || !node.parent) {
            continue;
        }
        if (!isSceneLocalId(*node.parent)) {
            continue;
        }

        const auto mapId = static_cast<world::MapId>(*node.parent + 1);
        const auto sceneId = static_cast<world::SceneId>(mapId * 100U);
        const auto coord = coordFor(node.local_id);

        auto idOpt = allocateInteractionId(mapId);
        if (!idOpt) {
            continue;
        }

        world::Interaction resource{};
        resource.id = *idOpt;
        resource.mapId = mapId;
        resource.sceneId = sceneId;
        resource.kind = world::InteractionKind::Resource;
        resource.coordLocal = coord;
        resource.coordGlobal = coord;
        resource.name = node.label.empty() ? "Resource" : node.label;
        resource.resourceType = parseResourceTypeTag(node).value_or(config.worlddb.resources.type);
        resource.capacity = config.worlddb.resources.capacity;
        resource.regenPerStep = config.worlddb.resources.regen_per_step;
        db.addInteraction(std::move(resource));
    }

    {
        std::set<std::tuple<world::MapId, world::MapId, bool>> seen;
        for (const auto& e : topology.edges) {
            const auto from = static_cast<world::MapId>(e.from + 1);
            const auto to = static_cast<world::MapId>(e.to + 1);
            if (!isSceneLocalId(e.from) || !isSceneLocalId(e.to)) {
                continue;
            }
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

    auto parsePortalTarget = [](const genesis::worldgen::NodeDraft& node) -> std::optional<std::size_t> {
        constexpr std::string_view prefix = "portal_target=";
        for (const auto& t : node.tags) {
            if (t.size() >= prefix.size() && t.compare(0, prefix.size(), prefix.data(), prefix.size()) == 0) {
                try {
                    return static_cast<std::size_t>(std::stoull(t.substr(prefix.size())));
                } catch (...) {
                    return std::nullopt;
                }
            }
        }
        return std::nullopt;
    };

    bool builtPortalFromNodes = false;
    if (hasInteractivePortals) {
        for (const auto& node : topology.nodes) {
            if (node.kind != genesis::worldgen::DraftNodeKind::InteractivePortal || !node.parent) {
                continue;
            }
            if (!isSceneLocalId(*node.parent)) {
                continue;
            }
            const auto targetLocal = parsePortalTarget(node);
            if (!targetLocal || !isSceneLocalId(*targetLocal)) {
                continue;
            }

            const auto entryMap = static_cast<world::MapId>(*node.parent + 1);
            const auto exitMap = static_cast<world::MapId>(*targetLocal + 1);
            if (entryMap == 0U || exitMap == 0U || entryMap == exitMap) {
                continue;
            }

            auto idOpt = allocateInteractionId(entryMap);
            if (!idOpt) {
                continue;
            }

            const auto entryCoord = coordFor(node.local_id);
            const auto exitCoord = coordFor(*targetLocal);

            world::Interaction portalInter{};
            portalInter.id = *idOpt;
            portalInter.mapId = entryMap;
            portalInter.sceneId = static_cast<world::SceneId>(entryMap * 100U);
            portalInter.kind = world::InteractionKind::Portal;
            portalInter.coordLocal = entryCoord;
            portalInter.coordGlobal = entryCoord;
            portalInter.name = node.label.empty() ? ("PortalTo_" + std::to_string(exitMap)) : node.label;
            db.addInteraction(portalInter);

            world::Portal p{};
            p.mapId = entryMap;
            p.interactionId = portalInter.id;
            p.targetMapId = exitMap;
            p.targetSceneId = static_cast<world::SceneId>(exitMap * 100U);
            p.targetCoord = exitCoord;
            db.addPortal(std::move(p));
            builtPortalFromNodes = true;
        }
    }

    if (!builtPortalFromNodes) {
        for (std::size_t index = 0; index < topology.portals.size(); ++index) {
            const auto& portal = topology.portals[index];
            const auto entryMap = static_cast<world::MapId>(portal.entry + 1);
            const auto exitMap = static_cast<world::MapId>(portal.exit + 1);
            if (!isSceneLocalId(portal.entry) || !isSceneLocalId(portal.exit)) {
                continue;
            }
            if (entryMap == 0U || exitMap == 0U || entryMap == exitMap) {
                continue;
            }

            const auto entryCoord = coordFor(portal.entry);
            const auto exitCoord = coordFor(portal.exit);

            auto idOpt = allocateInteractionId(entryMap);
            if (!idOpt) {
                continue;
            }

            world::Interaction portalInter{};
            portalInter.id = *idOpt;
            portalInter.mapId = entryMap;
            portalInter.sceneId = static_cast<world::SceneId>(entryMap * 100U);
            portalInter.kind = world::InteractionKind::Portal;
            portalInter.coordLocal = entryCoord;
            portalInter.coordGlobal = entryCoord;
            portalInter.name = "PortalTo_" + std::to_string(exitMap);
            db.addInteraction(portalInter);

            world::Portal p{};
            p.mapId = entryMap;
            p.interactionId = portalInter.id;
            p.targetMapId = exitMap;
            p.targetSceneId = static_cast<world::SceneId>(exitMap * 100U);
            p.targetCoord = exitCoord;
            db.addPortal(std::move(p));
        }
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
    [[nodiscard]] std::shared_ptr<const WorldAtlas> worldAtlas() const noexcept;
    [[nodiscard]] std::uint32_t worldVersion() const noexcept { return m_worldVersion.load(std::memory_order_acquire); }

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
    void updateWorldAtlas(std::shared_ptr<world::WorldDatabase> db, std::uint32_t worldVersion);

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

    std::atomic<std::uint32_t> m_worldVersion{0};
    mutable std::mutex m_worldMutex;
    std::shared_ptr<const WorldAtlas> m_worldAtlas;
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
        const auto version = m_worldVersion.fetch_add(1, std::memory_order_acq_rel) + 1;
        updateWorldAtlas(result.database, version);
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

std::shared_ptr<const WorldAtlas> Runtime::Impl::worldAtlas() const noexcept {
    std::scoped_lock lock(m_worldMutex);
    return m_worldAtlas;
}

void Runtime::Impl::updateWorldAtlas(std::shared_ptr<world::WorldDatabase> db, std::uint32_t worldVersion) {
    std::shared_ptr<const WorldAtlas> next;
    if (db) {
        next = std::make_shared<WorldAtlas>(buildWorldAtlasFromDatabase(*db, worldVersion));
    }
    std::scoped_lock lock(m_worldMutex);
    m_worldAtlas = std::move(next);
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

std::shared_ptr<const WorldAtlas> Runtime::worldAtlas() const noexcept {
    return m_impl->worldAtlas();
}

std::uint32_t Runtime::worldVersion() const noexcept {
    return m_impl->worldVersion();
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

std::optional<std::uint64_t> Runtime::enqueueCommandFromJson(const nlohmann::json& descriptor, std::string& errorMessage) {
    errorMessage.clear();

    if (!descriptor.is_object()) {
        errorMessage = "Command descriptor must be a JSON object";
        return std::nullopt;
    }

    const auto actionOpt = getStringField(descriptor, "action");
    if (!actionOpt || actionOpt->empty()) {
        errorMessage = "Command descriptor is missing string field: action";
        return std::nullopt;
    }

    const std::string action = *actionOpt;
    const std::string label = descriptor.value("label", action);

    json payload = descriptor;
    payload.erase("action");
    payload.erase("label");

    RuntimeEvent event{};
    event.kind = RuntimeEventKind::Command;
    event.label = label;
    if (!payload.empty()) {
        event.payloadJson = payload.dump();
    }

    if (action == "world.db.load" || action == "world.db.reload") {
        const auto folderOpt = getStringField(payload, "folder");
        if (!folderOpt || folderOpt->empty()) {
            errorMessage = "'" + action + "' requires string field: folder";
            return std::nullopt;
        }
        const auto folder = std::filesystem::path(*folderOpt);
        event.runtimeHandler = [folder](Runtime& runtime) {
            const auto result = runtime.loadWorldFromFile(folder);
            if (!result.success) {
                throw std::runtime_error(result.error.empty() ? "world.db.load failed" : result.error);
            }
        };
        return enqueueEvent(std::move(event));
    }

    if (action == "world.db.save") {
        const auto folderOpt = getStringField(payload, "folder");
        if (!folderOpt || folderOpt->empty()) {
            errorMessage = "'world.db.save' requires string field: folder";
            return std::nullopt;
        }
        const auto folder = std::filesystem::path(*folderOpt);
        event.runtimeHandler = [folder](Runtime& runtime) {
            const auto result = runtime.saveWorldToFile(folder);
            if (!result.success) {
                throw std::runtime_error(result.error.empty() ? "world.db.save failed" : result.error);
            }
        };
        return enqueueEvent(std::move(event));
    }

    if (action == "agent.create2d") {
        const auto mapIdOpt = getU32Field(payload, "mapId");
        const auto xOpt = getFloatField(payload, "x");
        const auto yOpt = getFloatField(payload, "y");
        if (!mapIdOpt || !xOpt || !yOpt) {
            errorMessage = "'agent.create2d' requires fields: mapId(uint), x(number), y(number)";
            return std::nullopt;
        }

        simulation::AgentSpawnParams2D params{};
        params.location.mapId = *mapIdOpt;
        params.location.x = *xOpt;
        params.location.y = *yOpt;

        if (payload.contains("move") && payload.at("move").is_object()) {
            const auto& move = payload.at("move");
            const auto tMapOpt = getU32Field(move, "mapId");
            const auto tXOpt = getFloatField(move, "x");
            const auto tYOpt = getFloatField(move, "y");
            const auto speedOpt = getFloatField(move, "speed");
            if (tMapOpt && tXOpt && tYOpt && speedOpt) {
                simulation::MovementCommand2D command{};
                command.targetMapId = *tMapOpt;
                command.targetX = *tXOpt;
                command.targetY = *tYOpt;
                command.speed = *speedOpt;
                params.initialMovement = command;
            }
        }

        event.runtimeHandler = [params](Runtime& runtime) mutable {
            const auto created = runtime.createAgent(params);
            if (created == 0U) {
                throw std::runtime_error("agent.create2d failed");
            }
        };
        event.onComplete = [params](RuntimeEventReport& report) {
            if (report.success) {
                report.message = "agent.create2d succeeded";
            } else if (report.message.empty()) {
                report.message = "agent.create2d failed";
            }
        };
        return enqueueEvent(std::move(event));
    }

    if (action == "agent.move2d") {
        const auto entityIdOpt = getU32Field(payload, "entityId");
        const auto mapIdOpt = getU32Field(payload, "mapId");
        const auto xOpt = getFloatField(payload, "x");
        const auto yOpt = getFloatField(payload, "y");
        const auto speedOpt = getFloatField(payload, "speed");
        if (!entityIdOpt || !mapIdOpt || !xOpt || !yOpt || !speedOpt) {
            errorMessage = "'agent.move2d' requires fields: entityId(uint), mapId(uint), x(number), y(number), speed(number)";
            return std::nullopt;
        }

        simulation::MovementCommand2D command{};
        command.targetMapId = *mapIdOpt;
        command.targetX = *xOpt;
        command.targetY = *yOpt;
        command.speed = *speedOpt;

        event.runtimeHandler = [entityId=*entityIdOpt, command](Runtime& runtime) {
            if (!runtime.setAgentMovementIntent(entityId, command)) {
                throw std::runtime_error("agent.move2d failed");
            }
        };
        return enqueueEvent(std::move(event));
    }

    if (action == "agent.stop2d") {
        const auto entityIdOpt = getU32Field(payload, "entityId");
        if (!entityIdOpt) {
            errorMessage = "'agent.stop2d' requires field: entityId(uint)";
            return std::nullopt;
        }
        event.runtimeHandler = [entityId=*entityIdOpt](Runtime& runtime) {
            if (!runtime.stopAgentMovement(entityId)) {
                throw std::runtime_error("agent.stop2d failed");
            }
        };
        return enqueueEvent(std::move(event));
    }

    if (action == "agent.teleport2d") {
        const auto entityIdOpt = getU32Field(payload, "entityId");
        const auto mapIdOpt = getU32Field(payload, "mapId");
        const auto xOpt = getFloatField(payload, "x");
        const auto yOpt = getFloatField(payload, "y");
        if (!entityIdOpt || !mapIdOpt || !xOpt || !yOpt) {
            errorMessage = "'agent.teleport2d' requires fields: entityId(uint), mapId(uint), x(number), y(number)";
            return std::nullopt;
        }

        simulation::AgentPose2D target{};
        target.mapId = *mapIdOpt;
        target.x = *xOpt;
        target.y = *yOpt;

        event.runtimeHandler = [entityId=*entityIdOpt, target](Runtime& runtime) {
            if (!runtime.teleportAgent(entityId, target)) {
                throw std::runtime_error("agent.teleport2d failed");
            }
        };
        return enqueueEvent(std::move(event));
    }

    if (action == "agent.delete2d") {
        const auto entityIdOpt = getU32Field(payload, "entityId");
        if (!entityIdOpt) {
            errorMessage = "'agent.delete2d' requires field: entityId(uint)";
            return std::nullopt;
        }
        event.runtimeHandler = [entityId=*entityIdOpt](Runtime& runtime) {
            if (!runtime.deleteAgent(entityId)) {
                throw std::runtime_error("agent.delete2d failed");
            }
        };
        return enqueueEvent(std::move(event));
    }

    if (action == "resource.consume") {
        const auto interactionIdOpt = getU32Field(payload, "interactionId");
        const auto amountOpt = getU32Field(payload, "amount");
        if (!interactionIdOpt || !amountOpt) {
            errorMessage = "'resource.consume' requires fields: interactionId(uint), amount(uint)";
            return std::nullopt;
        }
        event.runtimeHandler = [interactionId=*interactionIdOpt, amount=*amountOpt](Runtime& runtime) {
            runtime.consumeResource(interactionId, amount);
        };
        event.onComplete = [](RuntimeEventReport& report) {
            if (report.success && report.message.empty()) {
                report.message = "resource.consume succeeded";
            }
        };
        return enqueueEvent(std::move(event));
    }

    errorMessage = "Unsupported command action: " + action;
    return std::nullopt;
}

} // namespace Genesis::Runtime
