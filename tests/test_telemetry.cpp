#include <gtest/gtest.h>

#include "genesis/telemetry/TelemetryBuffer.hpp"

using genesis::telemetry::TelemetryBuffer;
using genesis::telemetry::TickTelemetry;

TEST(TelemetryBufferTest, MaintainsCapacity) {
    TelemetryBuffer buffer{3};

    for (std::uint64_t i = 0; i < 5; ++i) {
        TickTelemetry tick{};
        tick.step = i;
        buffer.push(std::move(tick));
    }

    const auto& entries = buffer.entries();
    ASSERT_EQ(entries.size(), 3U);
    EXPECT_EQ(entries[0].step, 2U);
    EXPECT_EQ(entries[2].step, 4U);
}

