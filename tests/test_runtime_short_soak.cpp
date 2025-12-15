#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string_view>

#include "genesis/runtime/Runtime.hpp"

namespace {

std::filesystem::path repoPath(std::string_view relative) {
    const std::filesystem::path root{GENESIS_TEST_SOURCE_DIR};
    return root / std::filesystem::path(relative);
}

[[nodiscard]] bool isFiniteFloat(float v) {
    return std::isfinite(static_cast<double>(v));
}

} // namespace

TEST(RuntimeShortSoak, WorldRemainsSaneForShortRun) {
    Genesis::Runtime::RuntimeConfig config{};
    config.initialWorldPath = repoPath("data/world_multiagent");

    Genesis::Runtime::Runtime runtime(config);

    const auto atlas = runtime.worldAtlas();
    ASSERT_NE(atlas, nullptr);
    EXPECT_EQ(atlas->schema_version, 2U);

    runtime.step(1);
    const auto* snapshot0 = runtime.latestSnapshot();
    ASSERT_NE(snapshot0, nullptr);
    ASSERT_EQ(snapshot0->telemetry.schema_version, 5U);
    ASSERT_FALSE(snapshot0->telemetry.agents.empty());
    ASSERT_FALSE(snapshot0->telemetry.resources.empty());

    bool sawNeeds = !snapshot0->telemetry.needs.empty();
    bool sawPlanner = !snapshot0->telemetry.plannerDecisions.empty();
    bool sawActions = !snapshot0->telemetry.actions.empty();

    std::uint64_t lastTelemetryStep = snapshot0->telemetry.step;

    constexpr std::uint64_t kSoakSteps = 5000;
    for (std::uint64_t i = 0; i < kSoakSteps; ++i) {
        runtime.step(1);
        const auto* snapshot = runtime.latestSnapshot();
        ASSERT_NE(snapshot, nullptr);

        const auto& telemetry = snapshot->telemetry;
        ASSERT_EQ(telemetry.schema_version, 5U);

        ASSERT_GE(telemetry.step, lastTelemetryStep);
        if (telemetry.step == lastTelemetryStep) {
            continue;
        }
        lastTelemetryStep = telemetry.step;

        for (const auto& agent : telemetry.agents) {
            EXPECT_TRUE(isFiniteFloat(agent.position.x));
            EXPECT_TRUE(isFiniteFloat(agent.position.y));
        }

        for (const auto& movement : telemetry.movements) {
            EXPECT_TRUE(isFiniteFloat(movement.position.x));
            EXPECT_TRUE(isFiniteFloat(movement.position.y));
            EXPECT_TRUE(isFiniteFloat(movement.target.x));
            EXPECT_TRUE(isFiniteFloat(movement.target.y));
            EXPECT_GE(movement.speed, 0.0f);
        }

        for (const auto& resource : telemetry.resources) {
            EXPECT_LE(resource.current, resource.capacity);
        }

        if (!telemetry.needs.empty()) {
            sawNeeds = true;
        }
        if (!telemetry.plannerDecisions.empty()) {
            sawPlanner = true;
        }
        if (!telemetry.actions.empty()) {
            sawActions = true;
        }
    }

    EXPECT_TRUE(sawNeeds);
    EXPECT_TRUE(sawPlanner);
    EXPECT_TRUE(sawActions);
}
