#include <gtest/gtest.h>

#include "genesis/runtime/Runtime.hpp"

TEST(RuntimeTest, GeneratesSnapshotsAfterStepping) {
    genesis::runtime::Runtime runtime({});
    runtime.step(5);

    const auto* snapshot = runtime.latestSnapshot();
    ASSERT_NE(snapshot, nullptr);
    EXPECT_GT(snapshot->step, 0U);
    EXPECT_FALSE(snapshot->resources.empty());
}

