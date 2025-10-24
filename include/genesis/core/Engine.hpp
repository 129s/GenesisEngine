#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>

#include <entt/entt.hpp>

#include "genesis/core/SimulationClock.hpp"
#include "genesis/telemetry/TelemetryBuffer.hpp"
#include "genesis/agents/Movement2D.hpp"
#include "genesis/simulation/Movement2DSystem.hpp"
#include "genesis/simulation/Scheduler.hpp"
#include "genesis/simulation/TelemetryCollector.hpp"
#include "genesis/simulation/ResourceSystem2D.hpp"

namespace genesis { namespace world { struct WorldLoadResult; class WorldDatabase; } }

namespace genesis::core {

class Engine {
public:
    Engine();

    void run(std::uint64_t maxSteps);
    void step(std::uint64_t steps = 1);

    [[nodiscard]] genesis::world::WorldLoadResult loadWorldFromFile(const std::filesystem::path& folder);

    [[nodiscard]] SimulationClock& clock() noexcept { return m_clock; }
    [[nodiscard]] const SimulationClock& clock() const noexcept { return m_clock; }

    [[nodiscard]] entt::registry& registry() noexcept { return m_registry; }
    [[nodiscard]] const entt::registry& registry() const noexcept { return m_registry; }

    [[nodiscard]] const telemetry::TelemetryBuffer& telemetry() const noexcept { return m_telemetry; }
    [[nodiscard]] const telemetry::TickTelemetry* latestTelemetry() const noexcept;

    using SnapshotCallback = std::function<void(const telemetry::TickTelemetry&)>;
    void setSnapshotCallback(SnapshotCallback callback);

    // 新：访问当前加载的世界数据库
    [[nodiscard]] std::shared_ptr<genesis::world::WorldDatabase> worldDatabase() const noexcept { return m_worldDb; }

private:
    void processStep(std::uint64_t stepIndex);
    void captureTelemetry(std::uint64_t stepIndex);
    void reportTelemetry(std::uint64_t stepIndex);
    void spawnDemoAgentsIfEmpty();

    SimulationClock m_clock;
    entt::registry m_registry;
    simulation::Movement2DSystem m_movement2d;
    simulation::ResourceSystem2D m_resource2d;
    simulation::Scheduler m_scheduler{&m_movement2d, &m_resource2d};
    simulation::TelemetryCollector m_telemetryCollector;
    telemetry::TelemetryBuffer m_telemetry;
    std::uint64_t m_lastTelemetryReportStep{0};
    static constexpr std::uint64_t kTelemetryReportInterval = 120;
    SnapshotCallback m_snapshotCallback;
    std::shared_ptr<genesis::world::WorldDatabase> m_worldDb;

public:
    // 资源接口（供 Runtime 命令使用）
    std::uint32_t consumeResource(std::uint32_t interactionId, std::uint32_t amount) { return m_resource2d.consume(interactionId, amount); }
    void initializeResourcesFromDatabase();
};

} // namespace genesis::core
