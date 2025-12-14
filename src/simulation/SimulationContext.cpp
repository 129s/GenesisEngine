#include "genesis/simulation/SimulationContext.hpp"

#include <utility>

#include "genesis/agents/Movement2D.hpp"
#include "genesis/telemetry/TelemetryBuffer.hpp"
#include "genesis/world/components/ResourceInventory.hpp"
#include "genesis/world/components/ResourceSpawn.hpp"

namespace genesis::simulation {

SimulationContext::SimulationContext()
    : m_needSatisfier({})
    , m_scheduler() {
    agents::NeedDescriptor hunger{};
    hunger.type = agents::NeedType::Hunger;
    hunger.minValue = 0.0f;
    hunger.maxValue = 100.0f;
    hunger.decayPerSecond = 8.0f;
    hunger.satisfiedThreshold = 30.0f;
    hunger.criticalThreshold = 80.0f;
    m_needSystem.setDefaultDescriptor(hunger);

    agents::NeedDescriptor thirst{};
    thirst.type = agents::NeedType::Thirst;
    thirst.minValue = 0.0f;
    thirst.maxValue = 100.0f;
    thirst.decayPerSecond = 10.0f;
    thirst.satisfiedThreshold = 30.0f;
    thirst.criticalThreshold = 80.0f;
    m_needSystem.setDefaultDescriptor(thirst);

    agents::NeedDescriptor social{};
    social.type = agents::NeedType::Social;
    social.minValue = 0.0f;
    social.maxValue = 100.0f;
    social.decayPerSecond = 4.0f;
    social.satisfiedThreshold = 30.0f;
    social.criticalThreshold = 80.0f;
    m_needSystem.setDefaultDescriptor(social);

    bindScheduler();
}

void SimulationContext::bindScheduler() {
    m_scheduler.setNeedSystem(&m_needSystem);
    m_scheduler.setNeedSatisfier(&m_needSatisfier);
    m_scheduler.setMovementSystem(&m_movementSystem);
    m_scheduler.setActionExecutor(m_actionExecutor.get());
    m_scheduler.setResourceSystem(m_resourceSystem.get());
    m_scheduler.setWorldDatabase(m_worldDatabase.get());
}

void SimulationContext::setWorldDatabase(std::shared_ptr<world::WorldDatabase> database) {
    if (m_resourceSystem) {
        m_resourceSystem->reset(m_registry);
    }
    m_actionExecutor.reset();
    m_resourceSystem.reset();
    m_registry.clear();
    m_worldDatabase = std::move(database);

    if (!m_worldDatabase) {
        bindScheduler();
        return;
    }

    m_resourceSystem = std::make_unique<world::system::ResourceSystem>(*m_worldDatabase, m_eventBus);
    m_resourceSystem->initialize(m_registry);
    m_actionExecutor = std::make_unique<agents::ActionExecutor>(*m_worldDatabase, *m_resourceSystem);
    bindScheduler();
}

void SimulationContext::tick(float deltaSeconds, std::uint64_t stepIndex) {
    m_scheduler.update(m_registry, deltaSeconds, stepIndex);
}

void SimulationContext::reset() {
    setWorldDatabase(nullptr);
}

std::uint32_t SimulationContext::createAgent(const AgentSpawnParams2D& params) {
    const auto entity = m_registry.create();
    agents::components::AgentLocation2D location{};
    location.mapId = params.location.mapId;
    location.x = params.location.x;
    location.y = params.location.y;
    m_registry.emplace<agents::components::AgentLocation2D>(entity, location);

    agents::NeedComponent needs{};
    m_needSystem.applyDefaults(needs);
    m_registry.emplace<agents::NeedComponent>(entity, std::move(needs));

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

bool SimulationContext::setAgentMovementIntent(std::uint32_t entityId, const MovementCommand2D& command) {
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

bool SimulationContext::clearAgentMovementIntent(std::uint32_t entityId) {
    const auto entity = toEntity(entityId);
    if (!m_registry.valid(entity)) {
        return false;
    }
    if (m_registry.any_of<agents::components::MovementIntent2D>(entity)) {
        m_registry.remove<agents::components::MovementIntent2D>(entity);
    }
    return true;
}

bool SimulationContext::teleportAgent(std::uint32_t entityId, const AgentPose2D& target) {
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

bool SimulationContext::deleteAgent(std::uint32_t entityId) {
    const auto entity = toEntity(entityId);
    if (!m_registry.valid(entity)) {
        return false;
    }
    m_registry.destroy(entity);
    return true;
}

bool SimulationContext::agentExists(std::uint32_t entityId) const {
    const auto entity = toEntity(entityId);
    return m_registry.valid(entity) && m_registry.all_of<agents::components::AgentLocation2D>(entity);
}

std::optional<AgentPose2D> SimulationContext::queryAgentPose(std::uint32_t entityId) const {
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

std::uint32_t SimulationContext::consumeResource(std::uint32_t interactionId, std::uint32_t amount) {
    if (!m_resourceSystem) {
        return 0;
    }

    std::optional<world::ResourceType> interactionType;
    m_resourceSystem->forEachSpawn(m_registry, [&](const auto& spawn, const auto&) {
        if (spawn.interaction == interactionId) {
            interactionType = spawn.type;
        }
    });

    if (!interactionType) {
        return 0;
    }

    return m_resourceSystem->consume(m_registry, *interactionType, amount, interactionId);
}

void SimulationContext::spawnDemoAgentsIfEmpty() {
    auto view = m_registry.view<agents::components::AgentLocation2D>();
    if (!view.empty()) {
        return;
    }

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

void SimulationContext::collectAgentSnapshots(std::vector<telemetry::AgentSnapshot>& out) const {
    out.clear();
    auto view = m_registry.view<agents::components::AgentLocation2D>();
    view.each([&](auto entity, const agents::components::AgentLocation2D& loc) {
        telemetry::AgentSnapshot snapshot{};
        snapshot.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
        snapshot.name = "Agent";
        snapshot.mapId = loc.mapId;
        snapshot.position.x = loc.x;
        snapshot.position.y = loc.y;
        out.push_back(std::move(snapshot));
    });
}

void SimulationContext::collectResourceSnapshots(std::vector<telemetry::ResourceSnapshot>& out) const {
    out.clear();
    if (!m_resourceSystem || !m_worldDatabase) {
        return;
    }

    m_resourceSystem->forEachSpawn(m_registry, [&](const auto& spawn, const auto& inventory) {
        telemetry::ResourceSnapshot snapshot{};
        snapshot.interactionId = spawn.interaction;
        snapshot.mapId = spawn.mapId;
        snapshot.name = spawn.name;
        snapshot.type = spawn.type;
        snapshot.capacity = inventory.capacity;
        snapshot.current = inventory.current;
        out.push_back(std::move(snapshot));
    });
}

namespace {
const char* needName(genesis::agents::NeedType type) {
    using genesis::agents::NeedType;
    switch (type) {
    case NeedType::Hunger:
        return "Hunger";
    case NeedType::Thirst:
        return "Thirst";
    case NeedType::Energy:
        return "Energy";
    case NeedType::Social:
        return "Social";
    case NeedType::Count:
        break;
    }
    return "Unknown";
}
} // namespace

void SimulationContext::collectNeedSnapshots(std::vector<telemetry::NeedSnapshot>& out) const {
    out.clear();
    auto view = m_registry.view<agents::NeedComponent>();
    view.each([&](auto entity, const agents::NeedComponent& needs) {
        needs.needs.forEach([&](const agents::NeedState& state, const agents::NeedDescriptor& descriptor) {
            telemetry::NeedSnapshot snap{};
            snap.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
            snap.needName = needName(state.type);
            snap.value = state.value;
            snap.critical = state.value >= descriptor.criticalThreshold;
            out.push_back(std::move(snap));
        });
    });
}

void SimulationContext::collectActionSnapshots(std::vector<telemetry::ActionSnapshot>& out) const {
    out.clear();
    auto view = m_registry.view<agents::ActionQueue>();
    view.each([&](auto entity, const agents::ActionQueue& queue) {
        telemetry::ActionSnapshot snap{};
        snap.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
        snap.queueLength = static_cast<std::uint32_t>(queue.tasks.size());
        if (!queue.tasks.empty()) {
            const auto& task = queue.tasks.front();
            snap.target = task.interaction;
            snap.speed = task.speed;
            snap.resource = task.resource;
            snap.amount = task.amount;
            snap.reliefPerUnit = task.reliefPerUnit;
            switch (task.type) {
            case agents::ActionType::MoveToInteraction:
                snap.currentAction = "MoveToInteraction";
                break;
            case agents::ActionType::ConsumeResource:
                snap.currentAction = "ConsumeResource";
                break;
            }
        }
        out.push_back(std::move(snap));
    });
}

void SimulationContext::collectPlannerSnapshots(std::vector<telemetry::PlannerSnapshot>& out) const {
    out.clear();
    auto view = m_registry.view<agents::components::PlannerDecision>();
    view.each([&](auto entity, const agents::components::PlannerDecision& decision) {
        telemetry::PlannerSnapshot snap{};
        snap.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
        snap.target = decision.target;
        snap.travelCost = decision.travelCost;
        snap.score = decision.score;
        out.push_back(std::move(snap));
    });
}

entt::entity SimulationContext::toEntity(std::uint32_t id) const noexcept {
    return static_cast<entt::entity>(id);
}

} // namespace genesis::simulation
