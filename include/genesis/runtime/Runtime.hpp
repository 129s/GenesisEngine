#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "genesis/runtime/SimulationSnapshot.hpp"
#include "genesis/runtime/SnapshotDiff.hpp"
#include "genesis/telemetry/TelemetryBuffer.hpp"
#include "genesis/runtime/RuntimeEvents.hpp"
#include "genesis/runtime/SimulationService.hpp"
#include "genesis/simulation/AgentApi.hpp"
#include "genesis/world/WorldDatabaseLoader.hpp"
#include "genesis/world/WorldDatabaseSaver.hpp"
#include "genesis/agents/Movement2D.hpp"

namespace genesis { namespace world { class WorldDatabase; } }

namespace Genesis::Runtime {

namespace simulation = genesis::simulation;
namespace telemetry = genesis::telemetry;
namespace world = Genesis::World;

struct RuntimeConfig {
    std::uint64_t bootstrapSteps{0};

    // 新：可选初始世界目录（包含 world.json + map_#.json）
    std::optional<std::filesystem::path> initialWorldPath;

    // 可选：自定义仿真服务工厂（缺省使用 EngineSimulationService）
    std::function<std::unique_ptr<SimulationService>()> simulationFactory;
};

class Runtime {
public:
    explicit Runtime(RuntimeConfig config = {});
    ~Runtime() = default;

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&) = delete;
    Runtime& operator=(Runtime&&) = delete;

    void step(std::uint64_t steps = 1);
    void run(std::uint64_t steps);

    [[nodiscard]] const SimulationSnapshot* latestSnapshot() const noexcept;
    [[nodiscard]] std::optional<SimulationSnapshotDiff> latestSnapshotDiff() const noexcept;
    [[nodiscard]] const std::optional<std::uint64_t>& lastSeed() const noexcept { return m_lastSeed; }

    struct WorldGenerationResult {
        bool success{false};
        std::filesystem::path configPath{};
        std::optional<std::filesystem::path> outputPath;
        struct Seed { std::uint64_t value{0}; } seed{};
        std::size_t locationCount{0};
        std::size_t edgeCount{0};
        double durationMs{0.0};
        std::vector<std::string> logs;
        std::string error;
    };

    // 已弃用：占位返回失败，避免编译器/调用处大改
    WorldGenerationResult generateWorldFromConfig(const std::filesystem::path& configPath, std::optional<std::uint64_t> seedOverride = std::nullopt, std::optional<std::filesystem::path> outputPath = std::nullopt);
    [[nodiscard]] const std::optional<WorldGenerationResult>& lastWorldGeneration() const noexcept { return m_lastWorldGen; }

    [[nodiscard]] world::WorldDbLoadResult loadWorldFromFile(const std::filesystem::path& path);
    [[nodiscard]] world::WorldDbSaveResult saveWorldToFile(const std::filesystem::path& path) const;

    [[nodiscard]] std::shared_ptr<class world::WorldDatabase> worldDatabase() const noexcept;

    std::uint64_t enqueueEvent(RuntimeEvent event);
    std::uint32_t createAgent(const simulation::AgentSpawnParams2D& params);
    bool setAgentMovementIntent(std::uint32_t entityId, const simulation::MovementCommand2D& command);
    bool teleportAgent(std::uint32_t entityId, const simulation::AgentPose2D& target);
    std::optional<simulation::AgentPose2D> agentPose(std::uint32_t entityId) const;

    [[deprecated("Use AgentSpawnParams2D overload")]]
    std::uint32_t createAgent2D(const genesis::agents::components::AgentLocation2D& location,
                                const std::optional<genesis::agents::components::MovementIntent2D>& intent = std::nullopt);
    [[deprecated("Use MovementCommand2D overload")]]
    bool setAgentMovementIntent(std::uint32_t entityId, const genesis::agents::components::MovementIntent2D& intent);
    bool stopAgentMovement(std::uint32_t entityId);
    [[deprecated("Use AgentPose2D overload")]]
    bool teleportAgent(std::uint32_t entityId, const genesis::agents::components::AgentLocation2D& target);
    bool deleteAgent(std::uint32_t entityId);
    std::uint32_t consumeResource(std::uint32_t interactionId, std::uint32_t amount);
    bool agentExists(std::uint32_t entityId) const;
    [[deprecated("Use agentPose()")]]
    std::optional<genesis::agents::components::AgentLocation2D> agentLocation(std::uint32_t entityId) const;

private:
    void drainPendingEvents();

    RuntimeConfig m_config;
    std::unique_ptr<SimulationService> m_simulation;
    std::optional<std::uint64_t> m_lastSeed;
    std::optional<WorldGenerationResult> m_lastWorldGen;
    std::atomic<std::uint64_t> m_snapshotVersion{0};
    SimulationSnapshotBuffer m_snapshotBuffer;
    std::atomic<std::uint64_t> m_nextEventId{1};
    mutable std::mutex m_eventMutex;
    std::queue<RuntimeEvent> m_pendingEvents;
    std::vector<RuntimeEventReport> m_eventsSinceLastSnapshot;
};

std::unique_ptr<Runtime> createRuntime(RuntimeConfig config = {});

} // namespace Genesis::Runtime
