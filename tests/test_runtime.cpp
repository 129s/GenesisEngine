#include <chrono>
#include <gtest/gtest.h>

#include "genesis/runtime/Runtime.hpp"

TEST(RuntimeTest, GeneratesSnapshotsAfterStepping) {
    genesis::runtime::Runtime runtime({});
    runtime.step(5);

    const auto* snapshot = runtime.latestSnapshot();
    ASSERT_NE(snapshot, nullptr);
    EXPECT_GT(snapshot->version, 0U);
    EXPECT_NE(snapshot->capturedAt, std::chrono::steady_clock::time_point{});
    const auto& tick = snapshot->telemetry;
    EXPECT_GT(tick.step, 0U);
    EXPECT_FALSE(tick.resources.empty());
}
