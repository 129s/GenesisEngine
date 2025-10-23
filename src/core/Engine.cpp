#include "genesis/core/Engine.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string_view>
#include <unordered_set>
#include <utility>

#include <spdlog/spdlog.h>

#include "genesis/world/WorldLoader.hpp"
#include "genesis/world/components/ResourceInventory.hpp"
#include "genesis/world/components/ResourceSpawn.hpp"
#include "genesis/agents/AgentComponents.hpp"
#include "genesis/agents/Personality.hpp"

namespace genesis::core {

namespace {

genesis::world::LocationId selectSpawnLocation(const genesis::world::WorldRegistry& world) {
    const auto spawns = world.allSpawns();
    // Prefer spawning directly at a valid resource location to guarantee reachability
    for (const auto& spawn : spawns) {
        if (const auto* node = world.findLocation(spawn.location); node && node->navigable) {
            return spawn.location;
        }
    }

    std::unordered_set<genesis::world::LocationId, genesis::world::LocationIdHasher> spawnLocations;
    spawnLocations.reserve(spawns.size());
    for (const auto& spawn : spawns) {
        spawnLocations.insert(spawn.location);
    }

    const auto nodes = world.locations();
    for (const auto& node : nodes) {
        if (node.navigable && node.kind == genesis::world::LocationKind::Point && !spawnLocations.contains(node.id)) {
            return node.id;
        }
    }

    for (const auto& node : nodes) {
        if (node.navigable && !spawnLocations.contains(node.id)) {
            return node.id;
        }
    }

    return genesis::world::InvalidLocation;
}

} // namespace

Engine::Engine()
    : m_clock(SimulationClock::duration{500})
    , m_movementSystem(m_world)
    , m_resourceSystem(m_world, m_eventBus)
    , m_actionExecutor(m_world, m_resourceSystem)
    , m_hungerPlanner(genesis::agents::NeedSatisfierConfig{
          .hungerUnitsPerRequest = 2,
          .hungerReliefPerUnit = 12.0f,
          .hungerPrepareMargin = 5.0f,
          .hungerPreferredLocator = [](entt::entity) {
              return genesis::world::InvalidLocation;
          },
      })
    , m_telemetry(512) {
    spdlog::info("GenesisEngine core initialized");
    configureNeedDefaults();
    m_resourceSystem.initialize(m_registry);
}

void Engine::setSnapshotCallback(SnapshotCallback callback) {
    m_snapshotCallback = std::move(callback);
}

void Engine::run(std::uint64_t maxSteps) {
    const auto delta = m_clock.stepDuration();
    std::uint64_t processed = 0;

    while (processed < maxSteps) {
        m_clock.advance(delta);

        if (!m_clock.stepReady()) {
            continue;
        }

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
    spdlog::debug("Processing simulation step {}", stepIndex);
    const float deltaSeconds = std::chrono::duration<float>(m_clock.stepDuration()).count();
    m_movementSystem.update(m_registry, deltaSeconds);
    m_resourceSystem.tick(m_registry, stepIndex);
    m_actionExecutor.update(m_registry, deltaSeconds);
    m_needSystem.update(m_registry, deltaSeconds);
    m_hungerDecisions.clear();
    genesis::planner::PlannerContext plannerContext{m_registry, m_world, m_resourceSystem, &m_hungerDecisions, &m_actionExecutor};
    m_hungerPlanner.evaluate(stepIndex, plannerContext);
    eventBus().updateAll();
    captureTelemetry(stepIndex);
}

void Engine::reloadWorld(genesis::world::LocationGraph graph) {
    applyWorldGraph(std::move(graph));
}

void Engine::applyWorldGraph(genesis::world::LocationGraph graph) {
    const auto locationCount = graph.nodes.size();
    const auto edgeCount = graph.edges.size();
    spdlog::info("Applying world graph ({} locations, {} edges)", locationCount, edgeCount);

    m_world.setGraph(std::move(graph));
    m_registry.clear();
    m_resourceSystem.reset(m_registry);
    m_needSystem = genesis::agents::NeedSystem{};
    m_resourceSystem.initialize(m_registry);
    configureNeedDefaults();
    spawnDemoAgents();

    m_hungerDecisions.clear();
    m_telemetry.clear();
    m_lastTelemetryReportStep = 0;
}

genesis::world::WorldLoadResult Engine::loadWorldFromFile(const std::filesystem::path& path) {
    auto result = genesis::world::loadWorldGraphFromFile(path);
    if (!result.success) {
        spdlog::error("Failed to load world from {}: {}", path.string(), result.error);
        return {false, std::move(result.error)};
    }

    applyWorldGraph(std::move(result.graph));
    return {true, {}};
}

genesis::world::WorldLoadResult Engine::loadWorldFromJsonString(std::string_view jsonData) {
    auto result = genesis::world::loadWorldGraphFromJsonString(jsonData);
    if (!result.success) {
        spdlog::error("Failed to load world from JSON: {}", result.error);
        return {false, std::move(result.error)};
    }

    applyWorldGraph(std::move(result.graph));
    return {true, {}};
}

void Engine::configureNeedDefaults() {
    using genesis::agents::NeedDescriptor;
    using genesis::agents::NeedType;

    NeedDescriptor hunger{};
    hunger.type = NeedType::Hunger;
    hunger.minValue = 0.0f;
    hunger.maxValue = 100.0f;
    hunger.decayPerSecond = 0.15f; // accelerate decay so agents need to eat within a few minutes
    hunger.satisfiedThreshold = 20.0f;
    hunger.criticalThreshold = 75.0f;
    m_needSystem.setDefaultDescriptor(hunger);

    NeedDescriptor energy{};
    energy.type = NeedType::Energy;
    energy.minValue = 0.0f;
    energy.maxValue = 100.0f;
    energy.decayPerSecond = 0.015f;
    energy.satisfiedThreshold = 10.0f;
    energy.criticalThreshold = 70.0f;
    m_needSystem.setDefaultDescriptor(energy);

    NeedDescriptor social{};
    social.type = NeedType::Social;
    social.minValue = 0.0f;
    social.maxValue = 100.0f;
    social.decayPerSecond = 0.02f;
    social.satisfiedThreshold = 15.0f;
    social.criticalThreshold = 60.0f;
    m_needSystem.setDefaultDescriptor(social);
}

void Engine::spawnDemoAgents() {
    using genesis::agents::NeedType;

    const auto spawnLocation = selectSpawnLocation(m_world);
    if (spawnLocation == genesis::world::InvalidLocation) {
        spdlog::warn("Unable to find navigable spawn location; demo agents will not be placed");
        return;
    }

    auto spawnOne = [&](std::string name, float hunger, float energy, float social, genesis::agents::AgentPersonalityBig5 persona) {
        auto entity = m_registry.create();
        auto& needs = m_registry.emplace<genesis::agents::NeedComponent>(entity);
        m_needSystem.applyDefaults(needs);
        auto& location = m_registry.emplace<genesis::agents::components::AgentLocation>(entity);
        location.location = spawnLocation;
        m_registry.emplace<genesis::agents::components::AgentName>(entity, genesis::agents::components::AgentName{std::move(name)});
        m_registry.emplace<genesis::agents::AgentPersonalityBig5>(entity, persona);
        needs.needs.setState(NeedType::Hunger, hunger);
        needs.needs.setState(NeedType::Energy, energy);
        needs.needs.setState(NeedType::Social, social);
        spdlog::info("Spawned demo agent '{}' at location {}", m_registry.get<genesis::agents::components::AgentName>(entity).name, spawnLocation.value);
    };

    spawnOne("Ava", 10.0f, 25.0f, 5.0f, genesis::agents::AgentPersonalityBig5{.openness=0.8f, .conscientiousness=0.4f, .extraversion=0.6f, .agreeableness=0.5f, .neuroticism=0.3f});
    spawnOne("Ben", 15.0f, 20.0f, 8.0f, genesis::agents::AgentPersonalityBig5{.openness=0.3f, .conscientiousness=0.8f, .extraversion=0.4f, .agreeableness=0.6f, .neuroticism=0.5f});
    spawnOne("Chloe", 8.0f, 30.0f, 12.0f, genesis::agents::AgentPersonalityBig5{.openness=0.5f, .conscientiousness=0.5f, .extraversion=0.9f, .agreeableness=0.6f, .neuroticism=0.2f});
}

Engine::ResourceRequestResult Engine::requestResource(world::ResourceType type, std::uint32_t amount, world::LocationId preferredLocation) {
    Engine::ResourceRequestResult result{};
    result.requested = amount;

    if (amount == 0U) {
        return result;
    }

    const auto consumed = m_resourceSystem.consume(m_registry, type, amount, preferredLocation);
    result.fulfilled = consumed;
    return result;
}

void Engine::captureTelemetry(std::uint64_t stepIndex) {
    telemetry::TickTelemetry tick{};
    tick.step = stepIndex;
    tick.stepSeconds = std::chrono::duration<float>(m_clock.stepDuration()).count();

    auto resourceView = m_registry.view<genesis::world::components::ResourceInventory, genesis::world::components::ResourceSpawn>();
    resourceView.each([&](auto, const auto& inventory, const auto& spawn) {
        telemetry::ResourceSnapshot snapshot{};
        snapshot.name = spawn.name;
        snapshot.type = spawn.type;
        snapshot.location = spawn.location;
        snapshot.current = inventory.current;
        snapshot.capacity = inventory.capacity;
        tick.resources.push_back(std::move(snapshot));
    });

    auto needView = m_registry.view<genesis::agents::NeedComponent>();
    needView.each([&](auto entity, const genesis::agents::NeedComponent& component) {
        const auto* hungerState = component.needs.state(genesis::agents::NeedType::Hunger);
        const auto* hungerDescriptor = component.needs.descriptor(genesis::agents::NeedType::Hunger);
        if (!hungerState || !hungerDescriptor) {
            return;
        }

        telemetry::NeedSnapshot hunger{};
        hunger.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
        hunger.needName = "Hunger";
        hunger.value = hungerState->value;
        hunger.critical = hungerState->value >= hungerDescriptor->criticalThreshold;
        tick.needs.push_back(std::move(hunger));
    });

    for (const auto& decision : m_hungerDecisions) {
        telemetry::PlannerSnapshot snapshot{};
        snapshot.entityId = static_cast<std::uint32_t>(entt::to_integral(decision.agent));
        snapshot.target = decision.target;
        snapshot.travelCost = decision.travelCost;
        snapshot.score = decision.score;
        tick.plannerDecisions.push_back(std::move(snapshot));
    }

    auto actionView = m_registry.view<genesis::agents::ActionQueue>();
    actionView.each([&](auto entity, const genesis::agents::ActionQueue& queue) {
        telemetry::ActionSnapshot snapshot{};
        snapshot.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
        snapshot.queueLength = static_cast<std::uint32_t>(queue.tasks.size());
        if (!queue.tasks.empty()) {
            const auto& task = queue.tasks.front();
            switch (task.type) {
            case genesis::agents::ActionType::MoveTo:
                snapshot.currentAction = "MoveTo";
                break;
            case genesis::agents::ActionType::ConsumeResource:
                snapshot.currentAction = "ConsumeResource";
                break;
            default:
                snapshot.currentAction = "Unknown";
                break;
            }
            snapshot.target = task.location;
            snapshot.speed = task.speed;
            snapshot.resource = task.resource;
            snapshot.amount = task.amount;
            snapshot.reliefPerUnit = task.reliefPerUnit;
        } else {
            snapshot.currentAction = "Idle";
        }
        tick.actions.push_back(std::move(snapshot));
    });

    auto agentView = m_registry.view<genesis::agents::components::AgentLocation>();
    agentView.each([&](auto entity, const genesis::agents::components::AgentLocation& location) {
        telemetry::AgentSnapshot snapshot{};
        snapshot.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
        if (const auto* name = m_registry.try_get<genesis::agents::components::AgentName>(entity)) {
            snapshot.name = name->name;
        }
        snapshot.location = location.location;
        snapshot.mapId = 1; // v1 语义：单一 Map，未来从 world 数据填充
        if (const auto* node = m_world.findLocation(location.location)) {
            if (node->coord_global.has_value()) {
                snapshot.position.x = static_cast<float>(node->coord_global->first);
                snapshot.position.y = static_cast<float>(node->coord_global->second);
            }
        }
        tick.agents.push_back(std::move(snapshot));
    });

    // Movement progress for Map interpolation
    auto moveView = m_registry.view<genesis::agents::components::MovementState, genesis::agents::components::AgentLocation>();
    moveView.each([&](auto entity, const genesis::agents::components::MovementState& state, const genesis::agents::components::AgentLocation& loc) {
        if (state.blocked || state.path.empty() || state.currentIndex >= state.path.size()) {
            return;
        }
        telemetry::MovementProgressSnapshot mp{};
        mp.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
        // From is the last reached node (current location), to is current segment target
        mp.from = loc.location;
        mp.to = state.path[state.currentIndex];
        if (state.segmentLength > 0.0f) {
            mp.t01 = std::clamp(state.traveledAlongEdge / state.segmentLength, 0.0f, 1.0f);
        } else {
            mp.t01 = 0.0f;
        }
        tick.movementProgress.push_back(mp);
    });

    if (m_snapshotCallback) {
        m_snapshotCallback(tick);
    }

    m_hungerDecisions.clear();

    m_telemetry.push(std::move(tick));
    reportTelemetry(stepIndex);
}

void Engine::reportTelemetry(std::uint64_t stepIndex) {
    if (stepIndex - m_lastTelemetryReportStep < kTelemetryReportInterval) {
        return;
    }

    m_lastTelemetryReportStep = stepIndex;

    const auto& entries = m_telemetry.entries();
    if (entries.empty()) {
        return;
    }

    const auto& latest = entries.back();

    std::uint32_t resourceCount = 0;
    std::uint32_t lowStockCount = 0;
    for (const auto& snapshot : latest.resources) {
        ++resourceCount;
        const auto threshold = static_cast<std::uint32_t>(snapshot.capacity * 0.2f);
        if (snapshot.current <= threshold) {
            ++lowStockCount;
        }
    }

    float hungerSum = 0.0f;
    std::uint32_t hungerCritical = 0;
    for (const auto& need : latest.needs) {
        if (need.needName == "Hunger") {
            hungerSum += need.value;
            if (need.critical) {
                ++hungerCritical;
            }
        }
    }

    const float hungerAvg = latest.needs.empty() ? 0.0f : hungerSum / static_cast<float>(latest.needs.size());

    float travelSum = 0.0f;
    for (const auto& decision : latest.plannerDecisions) {
        travelSum += decision.travelCost;
    }
    const float travelAvg = latest.plannerDecisions.empty() ? 0.0f : travelSum / static_cast<float>(latest.plannerDecisions.size());

    float queueSum = 0.0f;
    std::uint32_t consumingCount = 0;
    for (const auto& action : latest.actions) {
        queueSum += static_cast<float>(action.queueLength);
        if (action.currentAction == "ConsumeResource") {
            ++consumingCount;
        }
    }
    const std::uint32_t actionQueues = static_cast<std::uint32_t>(latest.actions.size());
    const float queueAvg = actionQueues == 0 ? 0.0f : queueSum / static_cast<float>(actionQueues);

    const std::uint32_t agentCount = static_cast<std::uint32_t>(latest.agents.size());

    spdlog::info("Telemetry step {}: resources={} low-stock={}, hunger avg={:.2f} critical={}, travel avg={:.2f}, actions={} consuming={}, queue avg={:.2f}, agents={}",
        stepIndex, resourceCount, lowStockCount, hungerAvg, hungerCritical, travelAvg, actionQueues, consumingCount, queueAvg, agentCount);
}
const genesis::telemetry::TickTelemetry* Engine::latestTelemetry() const noexcept {
    const auto& entries = m_telemetry.entries();
    if (entries.empty()) {
        return nullptr;
    }
    return &entries.back();
}

} // namespace genesis::core

