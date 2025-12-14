#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "genesis/runtime/Runtime.hpp"

namespace {

std::filesystem::path repoPath(std::string_view relative) {
    const std::filesystem::path root{GENESIS_TEST_SOURCE_DIR};
    return root / std::filesystem::path(relative);
}

std::optional<genesis::telemetry::ResourceSnapshot> findResource(const genesis::telemetry::TickTelemetry& telemetry,
                                                                 std::uint32_t interactionId) {
    for (const auto& resource : telemetry.resources) {
        if (resource.interactionId == interactionId) {
            return resource;
        }
    }
    return std::nullopt;
}

} // namespace

TEST(RuntimeJsonCommands, ResourceConsumeEnqueuedAsCommandProducesEventAndDiff) {
    Genesis::Runtime::RuntimeConfig config{};
    config.initialWorldPath = repoPath("data/world_new");
    Genesis::Runtime::Runtime runtime(config);

    runtime.step(1);
    const auto* before = runtime.latestSnapshot();
    ASSERT_NE(before, nullptr);
    const auto resource0 = findResource(before->telemetry, 1000U);
    ASSERT_TRUE(resource0.has_value());
    EXPECT_EQ(resource0->capacity, 50U);

    nlohmann::json cmd = {
        {"action", "resource.consume"},
        {"interactionId", 1000},
        {"amount", 10}
    };

    std::string error;
    const auto id = runtime.enqueueCommandFromJson(cmd, error);
    ASSERT_TRUE(id.has_value()) << error;

    runtime.step(1);
    const auto* after = runtime.latestSnapshot();
    ASSERT_NE(after, nullptr);

    bool sawReport = false;
    for (const auto& report : after->events) {
        if (report.id == *id) {
            sawReport = true;
            EXPECT_TRUE(report.success) << report.message;
            EXPECT_EQ(report.label, "resource.consume");
            break;
        }
    }
    EXPECT_TRUE(sawReport);

    const auto resource1 = findResource(after->telemetry, 1000U);
    ASSERT_TRUE(resource1.has_value());
    EXPECT_EQ(resource1->capacity, 50U);
    EXPECT_EQ(resource1->current, 42U); // 50 - 10 + regen(2)

    const auto diff = runtime.latestSnapshotDiff();
    ASSERT_TRUE(diff.has_value());
    EXPECT_FALSE(diff->executedEvents.empty());
}

