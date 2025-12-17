#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "genesis/runtime/Export.hpp"
#include "genesis/runtime/SimulationSnapshot.hpp"
#include "genesis/runtime/SnapshotDiff.hpp"
#include "genesis/runtime/WorldAtlas.hpp"
#include "genesis/telemetry/TelemetryBuffer.hpp"
#include "genesis/runtime/RuntimeEvents.hpp"
#include "genesis/runtime/SimulationService.hpp"
#include "genesis/simulation/AgentApi.hpp"
#include "genesis/simulation/Namespace.hpp"
#include "genesis/agents/Namespace.hpp"
#include "genesis/telemetry/Namespace.hpp"
#include "genesis/world/WorldDatabaseLoader.hpp"
#include "genesis/world/WorldDatabaseSaver.hpp"
#include "genesis/agents/Movement2D.hpp"

namespace genesis { namespace world { class WorldDatabase; } }

namespace Genesis::Runtime {

namespace simulation = Genesis::Simulation;
namespace telemetry = Genesis::Telemetry;
namespace world = Genesis::World;

struct RuntimeConfig {
    std::uint64_t bootstrapSteps{0};

    // 新：可选初始世界目录（包含 world.json + map_#.json）
    std::optional<std::filesystem::path> initialWorldPath;

    // 可选：自定义仿真服务工厂（缺省使用 EngineSimulationService）
    std::function<std::unique_ptr<SimulationService>()> simulationFactory;
};

class GENESIS_RUNTIME_API Runtime {
public:
    explicit Runtime(RuntimeConfig config = {});
    ~Runtime();

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&) = delete;
    Runtime& operator=(Runtime&&) = delete;

    void step(std::uint64_t steps = 1);
    void run(std::uint64_t steps);

    [[nodiscard]] const SimulationSnapshot* latestSnapshot() const noexcept;
    [[nodiscard]] std::optional<SimulationSnapshotDiff> latestSnapshotDiff() const noexcept;
    [[nodiscard]] std::shared_ptr<const WorldAtlas> worldAtlas() const noexcept;
    [[nodiscard]] std::uint32_t worldVersion() const noexcept;
    [[nodiscard]] const std::optional<std::uint64_t>& lastSeed() const noexcept;

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

    // 旧接口：直接执行世界生成并落盘（推荐通过 JSON 命令 `world.db.generate` 使用，便于事件追踪与脚本串联）。
    WorldGenerationResult generateWorldFromConfig(const std::filesystem::path& configPath, std::optional<std::uint64_t> seedOverride = std::nullopt, std::optional<std::filesystem::path> outputPath = std::nullopt);
    [[nodiscard]] const std::optional<WorldGenerationResult>& lastWorldGeneration() const noexcept;

    [[nodiscard]] world::WorldDbLoadResult loadWorldFromFile(const std::filesystem::path& path);
    [[nodiscard]] world::WorldDbSaveResult saveWorldToFile(const std::filesystem::path& path) const;

    [[nodiscard]] std::shared_ptr<class world::WorldDatabase> worldDatabase() const noexcept;

    std::uint64_t enqueueEvent(RuntimeEvent event);
    std::optional<std::uint64_t> enqueueCommandFromJson(const nlohmann::json& descriptor, std::string& errorMessage);
    // 脚本格式：{ name?:string, commands:[ {action:string, ..., waitForSuccess?:bool}, ... ] }
    // `waitForSuccess=true` 会在上一条命令成功后才提交下一条；失败将终止该脚本后续提交。
    bool enqueueCommandSequenceFromJson(const nlohmann::json& script, std::string& errorMessage);
    std::uint32_t createAgent(const simulation::AgentSpawnParams2D& params);
    bool setAgentMovementIntent(std::uint32_t entityId, const simulation::MovementCommand2D& command);
    bool teleportAgent(std::uint32_t entityId, const simulation::AgentPose2D& target);
    std::optional<simulation::AgentPose2D> agentPose(std::uint32_t entityId) const;

    [[deprecated("Use AgentSpawnParams2D overload")]]
    std::uint32_t createAgent2D(const Genesis::Agents::Components::AgentLocation2D& location,
                                const std::optional<Genesis::Agents::Components::MovementIntent2D>& intent = std::nullopt);
    [[deprecated("Use MovementCommand2D overload")]]
    bool setAgentMovementIntent(std::uint32_t entityId, const Genesis::Agents::Components::MovementIntent2D& intent);
    bool stopAgentMovement(std::uint32_t entityId);
    [[deprecated("Use AgentPose2D overload")]]
    bool teleportAgent(std::uint32_t entityId, const Genesis::Agents::Components::AgentLocation2D& target);
    bool deleteAgent(std::uint32_t entityId);
    std::uint32_t consumeResource(std::uint32_t interactionId, std::uint32_t amount);
    bool agentExists(std::uint32_t entityId) const;
    [[deprecated("Use agentPose()")]]
    std::optional<Genesis::Agents::Components::AgentLocation2D> agentLocation(std::uint32_t entityId) const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

GENESIS_RUNTIME_API std::unique_ptr<Runtime> createRuntime(RuntimeConfig config = {});

} // namespace Genesis::Runtime
