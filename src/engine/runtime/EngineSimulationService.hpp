#pragma once

#include <memory>

#include "genesis/core/Engine.hpp"
#include "genesis/runtime/SimulationService.hpp"

namespace Genesis::Runtime {

namespace simulation = genesis::simulation;
namespace world = Genesis::World;


class EngineSimulationService final : public SimulationService {
public:
    EngineSimulationService();
    ~EngineSimulationService() override = default;

    void setSnapshotCallback(SnapshotCallback callback) override;

    void step(std::uint64_t steps) override;
    void run(std::uint64_t steps) override;

    [[nodiscard]] world::WorldDbLoadResult loadWorld(const std::filesystem::path& folder) override;
    [[nodiscard]] std::shared_ptr<world::WorldDatabase> worldDatabase() const noexcept override;

    [[nodiscard]] std::uint32_t createAgent(const simulation::AgentSpawnParams2D& params) override;
    bool setAgentMovementIntent(std::uint32_t entityId, const simulation::MovementCommand2D& command) override;
    bool clearAgentMovementIntent(std::uint32_t entityId) override;
    bool teleportAgent(std::uint32_t entityId, const simulation::AgentPose2D& target) override;
    bool deleteAgent(std::uint32_t entityId) override;
    [[nodiscard]] bool agentExists(std::uint32_t entityId) const override;
    [[nodiscard]] std::optional<simulation::AgentPose2D> queryAgentPose(std::uint32_t entityId) const override;
    [[nodiscard]] std::uint32_t consumeResource(std::uint32_t interactionId, std::uint32_t amount) override;

private:
    genesis::core::Engine m_engine;
};

} // namespace Genesis::Runtime
