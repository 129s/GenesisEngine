#include "genesis/core/Engine.hpp"

#include <chrono>
#include <filesystem>
#include <utility>

#include <spdlog/spdlog.h>

#include "genesis/world/WorldBootstrap.hpp"
#include "genesis/world/WorldLoader.hpp"
#include "genesis/world/components/ResourceInventory.hpp"
#include "genesis/world/components/ResourceSpawn.hpp"

namespace genesis::core {

namespace {

std::filesystem::path findDataFile(const std::filesystem::path& relative) {
    constexpr int searchDepth = 4;
    auto current = std::filesystem::current_path();

    for (int i = 0; i < searchDepth; ++i) {
        const auto candidate = current / relative;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
        if (current.has_parent_path()) {
            current = current.parent_path();
        } else {
            break;
        }
    }

    return {};
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
          .hungerPreferredLocator = [](entt::entity) {
              return genesis::world::InvalidLocation;
          },
      })
    , m_telemetry(512) {
    spdlog::info("GenesisEngine core initialized");
    loadInitialWorld();
    m_resourceSystem.initialize(m_registry);
    configureNeedDefaults();
    spawnDemoAgents();
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

void Engine::loadInitialWorld() {
    static const std::filesystem::path defaultWorld{"data/world/demo_world.json"};

    if (const auto located = findDataFile(defaultWorld); !located.empty()) {
        spdlog::info("Loading world from {}", located.string());
        const auto result = genesis::world::loadWorldFromFile(located, m_world);
        if (result.success) {
            spdlog::info("World loaded ({} locations, {} spawns)",
                m_world.locationCount(), m_world.resourceSpawnCount());
            return;
        }

        spdlog::error("Failed to load world file: {}", result.error);
    } else {
        spdlog::warn("World file {} not found, using built-in demo world", defaultWorld.string());
    }

    m_world.setGraph(genesis::world::createDemoWorldGraph());
    spdlog::info("Fallback demo world loaded ({} locations, {} spawns)",
        m_world.locationCount(), m_world.resourceSpawnCount());
}

void Engine::configureNeedDefaults() {
    using genesis::agents::NeedDescriptor;
    using genesis::agents::NeedType;

    NeedDescriptor hunger{};
    hunger.type = NeedType::Hunger;
    hunger.minValue = 0.0f;
    hunger.maxValue = 100.0f;
    hunger.decayPerSecond = 0.03f; // reaches critical in roughly 45 minutes real-time at default step
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

    auto entity = m_registry.create();
    auto& needs = m_registry.emplace<genesis::agents::NeedComponent>(entity);
    m_needSystem.applyDefaults(needs);

    auto& location = m_registry.emplace<genesis::agents::components::AgentLocation>(entity);
    location.location = genesis::world::LocationId{6};

    needs.needs.setState(NeedType::Hunger, 10.0f);
    needs.needs.setState(NeedType::Energy, 25.0f);
    needs.needs.setState(NeedType::Social, 5.0f);

    spdlog::info("Spawned demo agent with baseline needs");
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
        snapshot.location = location.location;
        tick.agents.push_back(std::move(snapshot));
    });

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
} // namespace genesis::core








