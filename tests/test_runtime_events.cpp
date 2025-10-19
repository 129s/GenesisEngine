#include <gtest/gtest.h>

#include "genesis/runtime/Runtime.hpp"

TEST(RuntimeEventsTest, EnqueuedEventsExecuteAndAreCaptured) {
    genesis::runtime::Runtime runtime({});
    bool executed = false;
    bool completed = false;

    genesis::runtime::RuntimeEvent event{};
    event.kind = genesis::runtime::RuntimeEventKind::Marker;
    event.label = "ui-command";
    event.handler = [&](genesis::core::Engine&) {
        executed = true;
    };
    event.onComplete = [&](genesis::runtime::RuntimeEventReport& report) {
        completed = true;
        report.message = "ok";
    };

    const auto id = runtime.enqueueEvent(std::move(event));
    runtime.step(1);

    EXPECT_TRUE(executed);
    EXPECT_TRUE(completed);

    const auto* snapshot = runtime.latestSnapshot();
    ASSERT_NE(snapshot, nullptr);
    ASSERT_FALSE(snapshot->events.empty());
    const auto& record = snapshot->events.front();
    EXPECT_EQ(record.id, id);
    EXPECT_EQ(record.label, "ui-command");
    EXPECT_TRUE(record.success);
    EXPECT_EQ(record.message, "ok");

    const auto diffOpt = runtime.latestSnapshotDiff();
    ASSERT_TRUE(diffOpt.has_value());
    const auto& diff = diffOpt.value();
    ASSERT_FALSE(diff.executedEvents.empty());
    EXPECT_EQ(diff.executedEvents.front().id, id);
    EXPECT_EQ(diff.executedEvents.front().label, "ui-command");
    EXPECT_TRUE(diff.executedEvents.front().success);
    EXPECT_EQ(diff.executedEvents.front().message, "ok");
}
