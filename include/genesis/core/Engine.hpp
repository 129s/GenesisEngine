#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>

#include <entt/entt.hpp>

#include "genesis/core/SimulationClock.hpp"
#include "genesis/telemetry/TelemetryBuffer.hpp"
#include "genesis/agents/Movement2D.hpp"

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
    telemetry::TelemetryBuffer m_telemetry;
    std::uint64_t m_lastTelemetryReportStep{0};
    static constexpr std::uint64_t kTelemetryReportInterval = 120;
    SnapshotCallback m_snapshotCallback;
    std::shared_ptr<genesis::world::WorldDatabase> m_worldDb;
};

} // namespace genesis::core
