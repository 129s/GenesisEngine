#include <gtest/gtest.h>

#include <filesystem>
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

    bool sawActions = false;
    for (std::uint64_t i = 0; i < 200; ++i) {
        runtime.step(1);
        const auto* snapshot = runtime.latestSnapshot();
        ASSERT_NE(snapshot, nullptr);
        if (!snapshot->telemetry.actions.empty()) {
            sawActions = true;
            break;
        }
    }
    EXPECT_TRUE(sawActions);
}
