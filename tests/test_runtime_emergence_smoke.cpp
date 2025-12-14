#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string_view>

#include "genesis/runtime/Runtime.hpp"

namespace {

std::filesystem::path repoPath(std::string_view relative) {
    const std::filesystem::path root{GENESIS_TEST_SOURCE_DIR};
    return root / std::filesystem::path(relative);
}

} // namespace

TEST(RuntimeEmergenceSmoke, NeedsAreReportedAndActionsEventuallyAppear) {
    Genesis::Runtime::RuntimeConfig config{};
    config.initialWorldPath = repoPath("data/world_new");

    Genesis::Runtime::Runtime runtime(config);

    runtime.step(1);
    const auto* snapshot1 = runtime.latestSnapshot();
    ASSERT_NE(snapshot1, nullptr);
    EXPECT_FALSE(snapshot1->telemetry.needs.empty());

    bool sawPlanner = !snapshot1->telemetry.plannerDecisions.empty();
    bool sawActions = false;
    for (std::uint64_t i = 0; i < 400; ++i) {
        runtime.step(1);
        const auto* snapshot = runtime.latestSnapshot();
        ASSERT_NE(snapshot, nullptr);
        if (!snapshot->telemetry.plannerDecisions.empty()) {
            sawPlanner = true;
        }
        if (!snapshot->telemetry.actions.empty()) {
            sawActions = true;
            break;
        }
    }
    EXPECT_TRUE(sawPlanner);
    EXPECT_TRUE(sawActions);
}

TEST(RuntimeEmergenceSmoke, PlannerEventuallyTargetsWaterResource) {
    Genesis::Runtime::RuntimeConfig config{};
    config.initialWorldPath = repoPath("data/world_new");

    Genesis::Runtime::Runtime runtime(config);

    runtime.step(1);
    const auto* snapshot1 = runtime.latestSnapshot();
    ASSERT_NE(snapshot1, nullptr);
    ASSERT_FALSE(snapshot1->telemetry.resources.empty());

    std::optional<std::uint32_t> drinkInteraction;
    for (const auto& resource : snapshot1->telemetry.resources) {
        if (resource.type == genesis::world::ResourceType::Water && resource.capacity > 0U) {
            drinkInteraction = resource.interactionId;
            break;
        }
    }
    ASSERT_TRUE(drinkInteraction.has_value());

    bool sawDrinkDecision = false;
    for (std::uint64_t i = 0; i < 1500; ++i) {
        runtime.step(1);
        const auto* snapshot = runtime.latestSnapshot();
        ASSERT_NE(snapshot, nullptr);
        for (const auto& decision : snapshot->telemetry.plannerDecisions) {
            if (decision.target == *drinkInteraction) {
                sawDrinkDecision = true;
                break;
            }
        }
        if (sawDrinkDecision) {
            break;
        }
    }
    EXPECT_TRUE(sawDrinkDecision);
}
