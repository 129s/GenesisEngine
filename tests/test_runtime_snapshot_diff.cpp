#include <algorithm>

#include <gtest/gtest.h>

#include "genesis/runtime/SnapshotDiff.hpp"

using genesis::runtime::SnapshotChange;
using genesis::runtime::SnapshotChangeKind;

namespace {

genesis::telemetry::ResourceSnapshot makeResource(const char* name, genesis::world::LocationId id, std::uint32_t current, std::uint32_t capacity) {
    genesis::telemetry::ResourceSnapshot snapshot{};
    snapshot.name = name;
    snapshot.location = id;
    snapshot.current = current;
    snapshot.capacity = capacity;
    snapshot.type = genesis::world::ResourceType::Food;
    return snapshot;
}

} // namespace

TEST(SnapshotDiffTest, ReportsAddedModifiedAndRemovedResources) {
    genesis::runtime::SimulationSnapshot base{};
    base.version = 1;
    base.telemetry.step = 10;
    base.telemetry.resources.push_back(makeResource("Bakery", genesis::world::LocationId{1}, 10, 20));
    base.telemetry.resources.push_back(makeResource("Cafe", genesis::world::LocationId{2}, 6, 12));

    genesis::runtime::SimulationSnapshot target{};
    target.version = 2;
    target.telemetry.step = 11;
    target.telemetry.resources.push_back(makeResource("Bakery", genesis::world::LocationId{1}, 8, 20));
    target.telemetry.resources.push_back(makeResource("Tavern", genesis::world::LocationId{3}, 5, 10));

    const auto diff = genesis::runtime::diffSnapshots(&base, target);
    EXPECT_TRUE(diff.hasBase);
    EXPECT_EQ(diff.baseVersion, 1U);
    ASSERT_EQ(diff.resourceChanges.size(), 3U);

    const auto addedCount = std::count_if(diff.resourceChanges.begin(), diff.resourceChanges.end(), [](const SnapshotChange<genesis::telemetry::ResourceSnapshot>& change) {
        return change.kind == SnapshotChangeKind::Added;
    });
    EXPECT_EQ(addedCount, 1);

    const auto modifiedCount = std::count_if(diff.resourceChanges.begin(), diff.resourceChanges.end(), [](const SnapshotChange<genesis::telemetry::ResourceSnapshot>& change) {
        return change.kind == SnapshotChangeKind::Modified;
    });
    EXPECT_EQ(modifiedCount, 1);

    const auto removedCount = std::count_if(diff.resourceChanges.begin(), diff.resourceChanges.end(), [](const SnapshotChange<genesis::telemetry::ResourceSnapshot>& change) {
        return change.kind == SnapshotChangeKind::Removed;
    });
    EXPECT_EQ(removedCount, 1);
}

TEST(SnapshotDiffTest, TreatsFirstSnapshotAsAddedState) {
    genesis::runtime::SimulationSnapshot* base = nullptr;

    genesis::runtime::SimulationSnapshot target{};
    target.version = 1;
    target.telemetry.step = 1;
    target.telemetry.resources.push_back(makeResource("Bakery", genesis::world::LocationId{5}, 7, 9));

    const auto diff = genesis::runtime::diffSnapshots(base, target);
    EXPECT_FALSE(diff.hasBase);
    ASSERT_EQ(diff.resourceChanges.size(), 1U);
    EXPECT_EQ(diff.resourceChanges.front().kind, SnapshotChangeKind::Added);
    EXPECT_TRUE(diff.resourceChanges.front().before.has_value() == false);
    ASSERT_TRUE(diff.resourceChanges.front().after.has_value());
    EXPECT_EQ(diff.resourceChanges.front().after->name, "Bakery");
}
