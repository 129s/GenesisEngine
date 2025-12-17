#include "genesis/runtime/SnapshotDiff.hpp"

#include <string>
#include <unordered_map>
#include <utility>

namespace Genesis::Runtime {

namespace telemetry = genesis::telemetry;

namespace {

template <typename T>
const std::vector<T>& emptyVector() {
    static const std::vector<T> empty;
    return empty;
}

template <typename Snapshot, typename KeyFunc, typename EqualFunc>
std::vector<SnapshotChange<Snapshot>> diffCollection(
    const std::vector<Snapshot>* base,
    const std::vector<Snapshot>& target,
    KeyFunc keyFunc,
    EqualFunc equalFunc) {
    const auto& baseVec = base ? *base : emptyVector<Snapshot>();
    std::unordered_map<std::string, const Snapshot*> baseMap;
    baseMap.reserve(baseVec.size());
    for (const auto& entry : baseVec) {
        baseMap.emplace(keyFunc(entry), &entry);
    }

    std::vector<SnapshotChange<Snapshot>> changes;
    changes.reserve(target.size());

    for (const auto& entry : target) {
        const auto key = keyFunc(entry);
        if (auto it = baseMap.find(key); it != baseMap.end()) {
            const auto* baseEntry = it->second;
            if (!equalFunc(*baseEntry, entry)) {
                SnapshotChange<Snapshot> change{};
                change.kind = SnapshotChangeKind::Modified;
                change.before = *baseEntry;
                change.after = entry;
                changes.push_back(std::move(change));
            }
            baseMap.erase(it);
        } else {
            SnapshotChange<Snapshot> change{};
            change.kind = SnapshotChangeKind::Added;
            change.after = entry;
            changes.push_back(std::move(change));
        }
    }

    for (const auto& [key, baseEntry] : baseMap) {
        (void)key;
        SnapshotChange<Snapshot> change{};
        change.kind = SnapshotChangeKind::Removed;
        change.before = *baseEntry;
        changes.push_back(std::move(change));
    }

    return changes;
}

std::string resourceKey(const telemetry::ResourceSnapshot& snapshot) {
    return std::to_string(snapshot.interactionId);
}

bool resourceEqual(const telemetry::ResourceSnapshot& lhs, const telemetry::ResourceSnapshot& rhs) {
    return lhs.name == rhs.name && lhs.mapId == rhs.mapId && lhs.current == rhs.current && lhs.capacity == rhs.capacity &&
           lhs.consumed == rhs.consumed && lhs.produced == rhs.produced;
}

std::string needKey(const telemetry::NeedSnapshot& snapshot) {
    return std::to_string(snapshot.entityId) + ":" + snapshot.needName;
}

bool needEqual(const telemetry::NeedSnapshot& lhs, const telemetry::NeedSnapshot& rhs) {
    return lhs.entityId == rhs.entityId && lhs.needName == rhs.needName && lhs.value == rhs.value && lhs.critical == rhs.critical;
}

std::string plannerKey(const telemetry::PlannerSnapshot& snapshot) {
    return std::to_string(snapshot.entityId) + ":" + std::to_string(snapshot.target);
}

bool plannerEqual(const telemetry::PlannerSnapshot& lhs, const telemetry::PlannerSnapshot& rhs) {
    return lhs.entityId == rhs.entityId && lhs.target == rhs.target && lhs.travelCost == rhs.travelCost && lhs.score == rhs.score;
}

std::string actionKey(const telemetry::ActionSnapshot& snapshot) {
    return std::to_string(snapshot.entityId);
}

bool actionEqual(const telemetry::ActionSnapshot& lhs, const telemetry::ActionSnapshot& rhs) {
    return lhs.entityId == rhs.entityId && lhs.currentAction == rhs.currentAction && lhs.queueLength == rhs.queueLength && lhs.target == rhs.target &&
           lhs.targetEntityId == rhs.targetEntityId && lhs.speed == rhs.speed && lhs.resource == rhs.resource && lhs.amount == rhs.amount &&
           lhs.reliefPerUnit == rhs.reliefPerUnit;
}

std::string agentKey(const telemetry::AgentSnapshot& snapshot) {
    return std::to_string(snapshot.entityId);
}

bool agentEqual(const telemetry::AgentSnapshot& lhs, const telemetry::AgentSnapshot& rhs) {
    if (lhs.entityId != rhs.entityId) return false;
    if (lhs.name != rhs.name) return false;
    if (lhs.mapId != rhs.mapId) return false;
    if (lhs.position.x != rhs.position.x || lhs.position.y != rhs.position.y) return false;
    return true;
}

std::string movementKey(const telemetry::MovementSnapshot& snapshot) {
    return std::to_string(snapshot.entityId);
}

bool movementEqual(const telemetry::MovementSnapshot& lhs, const telemetry::MovementSnapshot& rhs) {
    if (lhs.entityId != rhs.entityId) return false;
    if (lhs.mapId != rhs.mapId) return false;
    if (lhs.position.x != rhs.position.x || lhs.position.y != rhs.position.y) return false;
    if (lhs.targetMapId != rhs.targetMapId) return false;
    if (lhs.target.x != rhs.target.x || lhs.target.y != rhs.target.y) return false;
    if (lhs.speed != rhs.speed) return false;
    return true;
}

} // namespace

SimulationSnapshotDiff diffSnapshots(const SimulationSnapshot* base, const SimulationSnapshot& target) {
    SimulationSnapshotDiff diff{};
    diff.targetVersion = target.version;
    diff.targetStep = target.telemetry.step;
    diff.capturedAt = target.capturedAt;
    diff.executedEvents = target.events;

    const telemetry::TickTelemetry* baseTelemetry = base ? &base->telemetry : nullptr;
    if (baseTelemetry) {
        diff.baseVersion = base->version;
        diff.baseStep = baseTelemetry->step;
        diff.hasBase = true;
    }

    diff.resourceChanges = diffCollection(baseTelemetry ? &baseTelemetry->resources : nullptr, target.telemetry.resources, resourceKey, resourceEqual);
    diff.needChanges = diffCollection(baseTelemetry ? &baseTelemetry->needs : nullptr, target.telemetry.needs, needKey, needEqual);
    diff.plannerChanges = diffCollection(baseTelemetry ? &baseTelemetry->plannerDecisions : nullptr, target.telemetry.plannerDecisions, plannerKey, plannerEqual);
    diff.actionChanges = diffCollection(baseTelemetry ? &baseTelemetry->actions : nullptr, target.telemetry.actions, actionKey, actionEqual);
    diff.agentChanges = diffCollection(baseTelemetry ? &baseTelemetry->agents : nullptr, target.telemetry.agents, agentKey, agentEqual);
    diff.movementChanges = diffCollection(baseTelemetry ? &baseTelemetry->movements : nullptr, target.telemetry.movements, movementKey, movementEqual);

    return diff;
}

} // namespace Genesis::Runtime
