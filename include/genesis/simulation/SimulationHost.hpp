#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "genesis/agents/Movement2D.hpp"
#include "genesis/simulation/AgentApi.hpp"
#include "genesis/simulation/SimulationContext.hpp"
#include "genesis/simulation/TelemetryCollector.hpp"
#include "genesis/telemetry/TelemetryBuffer.hpp"

namespace genesis::world {
class WorldDatabase;
}

namespace genesis::simulation {

class SimulationHost {
public:
    SimulationHost();

    void reset();

    void setWorldDatabase(std::shared_ptr<world::WorldDatabase> database);
    [[nodiscard]] std::shared_ptr<world::WorldDatabase> worldDatabase() const noexcept;

    void tick(float deltaSeconds, std::uint64_t stepIndex);

    telemetry::TickTelemetry captureTelemetry(std::uint64_t stepIndex, float stepSeconds);

    std::uint32_t createAgent(const AgentSpawnParams2D& params);
    bool setAgentMovementIntent(std::uint32_t entityId, const MovementCommand2D& command);
    bool teleportAgent(std::uint32_t entityId, const AgentPose2D& target);

    [[deprecated("Use AgentPose2D overload")]]
    bool teleportAgent(std::uint32_t entityId, const agents::components::AgentLocation2D& target);

    [[deprecated("Use AgentSpawnParams2D overloads")]]
    std::uint32_t createAgent2D(const agents::components::AgentLocation2D& location,
                                const std::optional<agents::components::MovementIntent2D>& intent = std::nullopt);
    bool setAgentMovementIntent(std::uint32_t entityId, const agents::components::MovementIntent2D& intent);
    bool clearAgentMovementIntent(std::uint32_t entityId);
    bool deleteAgent(std::uint32_t entityId);

    [[nodiscard]] bool agentExists(std::uint32_t entityId) const;
    [[nodiscard]] std::optional<AgentPose2D> queryAgentPose(std::uint32_t entityId) const;
    [[nodiscard]] std::optional<agents::components::AgentLocation2D> queryAgentLocation(std::uint32_t entityId) const;

    std::uint32_t consumeResource(std::uint32_t interactionId, std::uint32_t amount);

    void spawnDemoAgentsIfEmpty();

private:
    SimulationContext m_context;
    TelemetryCollector m_telemetryCollector;
    std::vector<telemetry::AgentSnapshot> m_agentScratch;
    std::vector<telemetry::ResourceSnapshot> m_resourceScratch;
    std::vector<telemetry::NeedSnapshot> m_needScratch;
    std::vector<telemetry::PlannerSnapshot> m_plannerScratch;
    std::vector<telemetry::ActionSnapshot> m_actionScratch;
    std::shared_ptr<world::WorldDatabase> m_worldDb;
};

} // namespace genesis::simulation
