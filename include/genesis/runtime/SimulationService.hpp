#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

#include "genesis/simulation/AgentApi.hpp"
#include "genesis/world/WorldDatabaseLoader.hpp"

namespace genesis::telemetry {
struct TickTelemetry;
}

namespace genesis::world {
class WorldDatabase;
}

namespace Genesis::Runtime {

namespace simulation = genesis::simulation;
namespace telemetry = genesis::telemetry;
namespace world = Genesis::World;


class SimulationService {
public:
    using SnapshotCallback = std::function<void(const telemetry::TickTelemetry&)>;

    virtual ~SimulationService() = default;

    virtual void setSnapshotCallback(SnapshotCallback callback) = 0;

    virtual void step(std::uint64_t steps) = 0;
    virtual void run(std::uint64_t steps) = 0;

    [[nodiscard]] virtual world::WorldDbLoadResult loadWorld(const std::filesystem::path& folder) = 0;
    [[nodiscard]] virtual std::shared_ptr<world::WorldDatabase> worldDatabase() const noexcept = 0;

    [[nodiscard]] virtual std::uint32_t createAgent(const simulation::AgentSpawnParams2D& params) = 0;
    virtual bool setAgentMovementIntent(std::uint32_t entityId, const simulation::MovementCommand2D& command) = 0;
    virtual bool clearAgentMovementIntent(std::uint32_t entityId) = 0;
    virtual bool teleportAgent(std::uint32_t entityId, const simulation::AgentPose2D& target) = 0;
    virtual bool deleteAgent(std::uint32_t entityId) = 0;
    [[nodiscard]] virtual bool agentExists(std::uint32_t entityId) const = 0;
    [[nodiscard]] virtual std::optional<simulation::AgentPose2D> queryAgentPose(std::uint32_t entityId) const = 0;
    [[nodiscard]] virtual std::uint32_t consumeResource(std::uint32_t interactionId, std::uint32_t amount) = 0;
};

} // namespace Genesis::Runtime
