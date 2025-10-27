#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <entt/entt.hpp>

#include "genesis/core/SimulationClock.hpp"
#include "genesis/telemetry/TelemetryBuffer.hpp"
#include "genesis/agents/Movement2D.hpp"
#include "genesis/simulation/SimulationContext.hpp"
#include "genesis/simulation/TelemetryCollector.hpp"
#include "genesis/world/WorldDatabaseLoader.hpp"

namespace genesis { namespace world { class WorldDatabase; } }

namespace genesis::core {

class Engine {
public:
    Engine();

    void run(std::uint64_t maxSteps);
    void step(std::uint64_t steps = 1);

    [[nodiscard]] genesis::world::WorldDbLoadResult loadWorldFromFile(const std::filesystem::path& folder);

    [[nodiscard]] SimulationClock& clock() noexcept { return m_clock; }
    [[nodiscard]] const SimulationClock& clock() const noexcept { return m_clock; }

    [[nodiscard]] const telemetry::TelemetryBuffer& telemetry() const noexcept { return m_telemetry; }
    [[nodiscard]] const telemetry::TickTelemetry* latestTelemetry() const noexcept;

    using SnapshotCallback = std::function<void(const telemetry::TickTelemetry&)>;
    void setSnapshotCallback(SnapshotCallback callback);

    // 新：访问当前加载的世界数据库
    [[nodiscard]] std::shared_ptr<genesis::world::WorldDatabase> worldDatabase() const noexcept { return m_worldDb; }

    std::uint32_t createAgent2D(const genesis::agents::components::AgentLocation2D& location,
                                const std::optional<genesis::agents::components::MovementIntent2D>& intent = std::nullopt);
    bool setAgentMovementIntent(std::uint32_t entityId, const genesis::agents::components::MovementIntent2D& intent);
    bool clearAgentMovementIntent(std::uint32_t entityId);
    bool teleportAgent(std::uint32_t entityId, const genesis::agents::components::AgentLocation2D& target);
    bool deleteAgent(std::uint32_t entityId);
    bool agentExists(std::uint32_t entityId) const;
    std::optional<genesis::agents::components::AgentLocation2D> queryAgentLocation(std::uint32_t entityId) const;

    std::uint32_t consumeResource(std::uint32_t interactionId, std::uint32_t amount);

private:
    [[nodiscard]] entt::entity toEntity(std::uint32_t id) const noexcept;
    void destroyAllAgents();
    void processStep(std::uint64_t stepIndex);
    void captureTelemetry(std::uint64_t stepIndex);
    void reportTelemetry(std::uint64_t stepIndex);
    void spawnDemoAgentsIfEmpty();

    SimulationClock m_clock;
    entt::registry m_registry;
    simulation::SimulationContext m_simulation;
    simulation::TelemetryCollector m_telemetryCollector;
    telemetry::TelemetryBuffer m_telemetry;
    std::vector<telemetry::ResourceSnapshot> m_resourceScratch;
    std::uint64_t m_lastTelemetryReportStep{0};
    static constexpr std::uint64_t kTelemetryReportInterval = 120;
    SnapshotCallback m_snapshotCallback;
    std::shared_ptr<genesis::world::WorldDatabase> m_worldDb;

    void initializeResourcesFromDatabase();
};

} // namespace genesis::core
