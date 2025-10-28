#include <gtest/gtest.h>

#include "genesis/runtime/Runtime.hpp"
#include "genesis/telemetry/TelemetryBuffer.hpp"

namespace {

class DummySimulationService : public Genesis::Runtime::SimulationService {
public:
    void setSnapshotCallback(SnapshotCallback callback) override {
        snapshotCallback = std::move(callback);
        snapshotHooked = true;
    }

    void step(std::uint64_t steps) override {
        stepCalls += steps;
        if (snapshotCallback) {
            genesis::telemetry::TickTelemetry tick{};
            tick.step = ++lastStep;
            snapshotCallback(tick);
        }
    }

    void run(std::uint64_t steps) override {
        runCalls += steps;
    }

    [[nodiscard]] genesis::world::WorldDbLoadResult loadWorld(const std::filesystem::path&) override {
        ++loadCalls;
        genesis::world::WorldDbLoadResult result{};
        result.success = true;
        return result;
    }

    [[nodiscard]] std::shared_ptr<genesis::world::WorldDatabase> worldDatabase() const noexcept override {
        return {};
    }

    [[nodiscard]] std::uint32_t createAgent(const genesis::simulation::AgentSpawnParams2D&) override {
        ++createAgentCalls;
        return 42U;
    }

    bool setAgentMovementIntent(std::uint32_t, const genesis::simulation::MovementCommand2D&) override {
        ++setIntentCalls;
        return true;
    }

    bool clearAgentMovementIntent(std::uint32_t) override {
        ++clearIntentCalls;
        return true;
    }

    bool teleportAgent(std::uint32_t, const genesis::simulation::AgentPose2D&) override {
        ++teleportCalls;
        return true;
    }

    bool deleteAgent(std::uint32_t) override {
        ++deleteCalls;
        return true;
    }

    [[nodiscard]] bool agentExists(std::uint32_t) const override {
        ++agentExistsChecks;
        return true;
    }

    [[nodiscard]] std::optional<genesis::simulation::AgentPose2D> queryAgentPose(std::uint32_t) const override {
        ++queryPoseCalls;
        return genesis::simulation::AgentPose2D{};
    }

    [[nodiscard]] std::uint32_t consumeResource(std::uint32_t, std::uint32_t amount) override {
        consumedAmount += amount;
        return amount;
    }

    bool snapshotHooked{false};
    std::uint64_t stepCalls{0};
    std::uint64_t runCalls{0};
    std::uint64_t loadCalls{0};
    std::uint64_t createAgentCalls{0};
    std::uint64_t setIntentCalls{0};
    std::uint64_t clearIntentCalls{0};
    std::uint64_t teleportCalls{0};
    std::uint64_t deleteCalls{0};
    mutable std::uint64_t agentExistsChecks{0};
    mutable std::uint64_t queryPoseCalls{0};
    std::uint64_t consumedAmount{0};

private:
    SnapshotCallback snapshotCallback;
    std::uint64_t lastStep{0};
};

} // namespace

TEST(RuntimeSimulationServiceTest, UsesInjectedSimulationService) {
    DummySimulationService* stub = nullptr;

    Genesis::Runtime::RuntimeConfig config{};
    config.simulationFactory = [&]() {
        auto ptr = std::make_unique<DummySimulationService>();
        stub = ptr.get();
        return ptr;
    };

    Genesis::Runtime::Runtime runtime(config);

    ASSERT_NE(stub, nullptr);
    EXPECT_TRUE(stub->snapshotHooked);

    auto loadResult = runtime.loadWorldFromFile("does-not-exist");
    EXPECT_TRUE(loadResult.success);
    EXPECT_EQ(stub->loadCalls, 1U);

    runtime.step(3);
    EXPECT_EQ(stub->stepCalls, 3U);

    genesis::simulation::AgentSpawnParams2D params{};
    params.location.mapId = 1;
    params.location.x = 0.0f;
    params.location.y = 0.0f;

    const auto agentId = runtime.createAgent(params);
    EXPECT_EQ(agentId, 42U);
    EXPECT_EQ(stub->createAgentCalls, 1U);

    genesis::simulation::MovementCommand2D command{};
    command.targetMapId = 1;
    command.targetX = 5.0f;
    command.targetY = 3.0f;
    command.speed = 2.0f;

    EXPECT_TRUE(runtime.setAgentMovementIntent(agentId, command));
    EXPECT_EQ(stub->setIntentCalls, 1U);

    EXPECT_TRUE(runtime.stopAgentMovement(agentId));
    EXPECT_EQ(stub->clearIntentCalls, 1U);

    genesis::simulation::AgentPose2D target{};
    target.mapId = 1;
    target.x = 4.0f;
    target.y = 2.0f;

    EXPECT_TRUE(runtime.teleportAgent(agentId, target));
    EXPECT_EQ(stub->teleportCalls, 1U);

    EXPECT_TRUE(runtime.agentExists(agentId));
    EXPECT_EQ(stub->agentExistsChecks, 1U);

    const auto pose = runtime.agentPose(agentId);
    ASSERT_TRUE(pose.has_value());
    EXPECT_EQ(stub->queryPoseCalls, 1U);

    EXPECT_EQ(runtime.consumeResource(10U, 5U), 5U);
    EXPECT_EQ(stub->consumedAmount, 5U);

    EXPECT_TRUE(runtime.deleteAgent(agentId));
    EXPECT_EQ(stub->deleteCalls, 1U);
}
