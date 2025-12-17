#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

#include "genesis/runtime/SimulationSnapshot.hpp"

namespace Genesis::Runtime {

namespace telemetry = Genesis::Telemetry;


enum class SnapshotChangeKind {
    Added,
    Removed,
    Modified
};

template <typename SnapshotType>
struct SnapshotChange {
    SnapshotChangeKind kind{SnapshotChangeKind::Modified};
    std::optional<SnapshotType> before;
    std::optional<SnapshotType> after;
};

struct SimulationSnapshotDiff {
    std::uint64_t baseVersion{0};
    std::uint64_t targetVersion{0};
    std::optional<std::uint64_t> baseStep;
    std::uint64_t targetStep{0};
    std::chrono::steady_clock::time_point capturedAt{};
    bool hasBase{false};

    std::vector<SnapshotChange<telemetry::ResourceSnapshot>> resourceChanges;
    std::vector<SnapshotChange<telemetry::NeedSnapshot>> needChanges;
    std::vector<SnapshotChange<telemetry::PlannerSnapshot>> plannerChanges;
    std::vector<SnapshotChange<telemetry::ActionSnapshot>> actionChanges;
    std::vector<SnapshotChange<telemetry::AgentSnapshot>> agentChanges;
    std::vector<SnapshotChange<telemetry::MovementSnapshot>> movementChanges;
    std::vector<RuntimeEventReport> executedEvents;

    [[nodiscard]] bool empty() const noexcept {
        return resourceChanges.empty() && needChanges.empty() && plannerChanges.empty() && actionChanges.empty() && agentChanges.empty() &&
               movementChanges.empty() && executedEvents.empty();
    }
};

[[nodiscard]] SimulationSnapshotDiff diffSnapshots(const SimulationSnapshot* base, const SimulationSnapshot& target);

} // namespace Genesis::Runtime
