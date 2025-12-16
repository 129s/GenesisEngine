#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <entt/entt.hpp>

#include "genesis/agents/ActionSystem.hpp"
#include "genesis/agents/LearningSystem.hpp"
#include "genesis/agents/NeedSatisfier.hpp"
#include "genesis/agents/NeedSystem.hpp"
#include "genesis/agents/Planner.hpp"
#include "genesis/messaging/EventBus.hpp"
#include "genesis/simulation/AgentApi.hpp"
#include "genesis/simulation/Movement2DSystem.hpp"
#include "genesis/simulation/Scheduler.hpp"
#include "genesis/telemetry/TelemetryBuffer.hpp"
#include "genesis/world/WorldDatabase.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

namespace genesis::simulation {

class SimulationContext {
public:
    SimulationContext();

    void setWorldDatabase(std::shared_ptr<world::WorldDatabase> database);
    void tick(float deltaSeconds, std::uint64_t stepIndex);
    void reset();

    [[nodiscard]] bool hasWorld() const noexcept { return static_cast<bool>(m_worldDatabase); }
    [[nodiscard]] const std::shared_ptr<world::WorldDatabase>& worldDatabase() const noexcept { return m_worldDatabase; }

    [[nodiscard]] world::system::ResourceSystem* resourceSystem() noexcept { return m_resourceSystem.get(); }
    [[nodiscard]] const world::system::ResourceSystem* resourceSystem() const noexcept { return m_resourceSystem.get(); }
    [[nodiscard]] agents::ActionExecutor* actionExecutor() noexcept { return m_actionExecutor.get(); }

    std::uint32_t createAgent(const AgentSpawnParams2D& params);
    bool setAgentMovementIntent(std::uint32_t entityId, const MovementCommand2D& command);
    bool clearAgentMovementIntent(std::uint32_t entityId);
    bool teleportAgent(std::uint32_t entityId, const AgentPose2D& target);
    bool deleteAgent(std::uint32_t entityId);
    [[nodiscard]] bool agentExists(std::uint32_t entityId) const;
    [[nodiscard]] std::optional<AgentPose2D> queryAgentPose(std::uint32_t entityId) const;
    std::uint32_t consumeResource(std::uint32_t interactionId, std::uint32_t amount);

    void spawnDemoAgentsIfEmpty();

    void collectAgentSnapshots(std::vector<telemetry::AgentSnapshot>& out) const;
    void collectResourceSnapshots(std::vector<telemetry::ResourceSnapshot>& out) const;
    void collectNeedSnapshots(std::vector<telemetry::NeedSnapshot>& out) const;
    void collectActionSnapshots(std::vector<telemetry::ActionSnapshot>& out) const;
    void collectWorkshopAttemptSnapshots(std::vector<telemetry::WorkshopAttemptSnapshot>& out) const;
    void collectResourceAttemptSnapshots(std::vector<telemetry::ResourceAttemptSnapshot>& out) const;
    void collectPlannerSnapshots(std::vector<telemetry::PlannerSnapshot>& out) const;

    messaging::EventBus& eventBus() noexcept { return m_eventBus; }
    const messaging::EventBus& eventBus() const noexcept { return m_eventBus; }

private:
    void bindScheduler();
    [[nodiscard]] entt::entity toEntity(std::uint32_t id) const noexcept;

    entt::registry m_registry;
    std::shared_ptr<world::WorldDatabase> m_worldDatabase;
    messaging::EventBus m_eventBus;
    agents::NeedSystem m_needSystem;
    agents::NeedSatisfier m_needSatisfier;
    agents::LearningSystem m_learningSystem;
    std::unique_ptr<agents::ActionExecutor> m_actionExecutor;
    std::unique_ptr<world::system::ResourceSystem> m_resourceSystem;
    Movement2DSystem m_movementSystem;
    Scheduler m_scheduler;
};

} // namespace genesis::simulation
