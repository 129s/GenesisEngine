#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "genesis/world/WorldTypes.hpp"

namespace genesis::telemetry {

struct ResourceSnapshot {
    std::string name;
    genesis::world::ResourceType type{genesis::world::ResourceType::Food};
    genesis::world::LocationId location{genesis::world::InvalidLocation};
    std::uint32_t current{0};
    std::uint32_t capacity{0};
};

struct NeedSnapshot {
    std::uint32_t entityId{0};
    std::string needName;
    float value{0.0f};
    bool critical{false};
};

struct PlannerSnapshot {
    std::uint32_t entityId{0};
    genesis::world::LocationId target{genesis::world::InvalidLocation};
    float travelCost{0.0f};
    float score{0.0f};
};

struct ActionSnapshot {
    std::uint32_t entityId{0};
    std::string currentAction;
    std::uint32_t queueLength{0};
    genesis::world::LocationId target{genesis::world::InvalidLocation};
    float speed{0.0f};
    genesis::world::ResourceType resource{genesis::world::ResourceType::Food};
    std::uint32_t amount{0};
    float reliefPerUnit{0.0f};
};

struct AgentSnapshot {
    std::uint32_t entityId{0};
    std::string name;
    genesis::world::LocationId location{genesis::world::InvalidLocation};
};

struct MovementProgressSnapshot {
    std::uint32_t entityId{0};
    genesis::world::LocationId from{genesis::world::InvalidLocation};
    genesis::world::LocationId to{genesis::world::InvalidLocation};
    float t01{0.0f};
};

struct TickTelemetry {
    std::uint64_t step{0};
    float stepSeconds{0.0f};
    std::vector<ResourceSnapshot> resources;
    std::vector<NeedSnapshot> needs;
    std::vector<PlannerSnapshot> plannerDecisions;
    std::vector<ActionSnapshot> actions;
    std::vector<AgentSnapshot> agents;
    std::vector<MovementProgressSnapshot> movementProgress;
};

class TelemetryBuffer {
public:
    explicit TelemetryBuffer(std::size_t maxEntries = 256);

    void push(TickTelemetry entry);
    void clear() noexcept;

    [[nodiscard]] const std::deque<TickTelemetry>& entries() const noexcept { return m_entries; }

private:
    std::size_t m_maxEntries;
    std::deque<TickTelemetry> m_entries;
};

} // namespace genesis::telemetry

