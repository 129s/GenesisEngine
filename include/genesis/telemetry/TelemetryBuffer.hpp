#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "genesis/telemetry/SchemaVersions.hpp"
#include "genesis/telemetry/Namespace.hpp"
#include "genesis/world/WorldTypes.hpp"
#include "genesis/world/WorldDatabase.hpp"

namespace genesis::telemetry {

struct Float2 {
    float x{0.0f};
    float y{0.0f};
};

struct ResourceSnapshot {
    std::uint32_t interactionId{0};
    std::uint32_t mapId{0};
    std::string name;
    genesis::world::ResourceType type{genesis::world::ResourceType::Food};
    std::uint32_t current{0};
    std::uint32_t capacity{0};
    // 本 tick 内的变化量（由运行时写入；用于 Soak/对比，不作为世界状态的唯一来源）。
    std::uint32_t consumed{0};
    std::uint32_t produced{0};
    std::uint32_t decayed{0};
};

struct NeedSnapshot {
    std::uint32_t entityId{0};
    std::string needName;
    float value{0.0f};
    bool critical{false};
};

struct PlannerSnapshot {
    std::uint32_t entityId{0};
    genesis::world::InteractionId target{0};
    float travelCost{0.0f};
    float score{0.0f};
};

struct ActionSnapshot {
    std::uint32_t entityId{0};
    std::string currentAction;
    std::uint32_t queueLength{0};
    genesis::world::InteractionId target{0};
    std::uint32_t targetEntityId{0}; // 仅对 SocializeWithAgent 有意义
    float speed{0.0f};
    genesis::world::ResourceType resource{genesis::world::ResourceType::Food};
    std::uint32_t amount{0};
    float reliefPerUnit{0.0f};
};

struct WorkshopAttemptSnapshot {
    std::uint32_t entityId{0};
    genesis::world::InteractionId interactionId{0};
    genesis::world::ResourceType outputType{genesis::world::ResourceType::Food};
    std::uint32_t wantedBatches{0};
    std::uint32_t wantedUnits{0};
    std::uint32_t producedUnits{0};
    // 空字符串表示成功；非空表示失败原因（见 docs/architecture/foundation/telemetry-schema.md）。
    std::string failureReason;
};

struct ResourceAttemptSnapshot {
    std::uint32_t entityId{0};
    std::string action; // ConsumeResource / TakeResource
    genesis::world::InteractionId interactionId{0};
    genesis::world::ResourceType resourceType{genesis::world::ResourceType::Food};
    std::uint32_t wantedUnits{0};
    std::uint32_t obtainedUnits{0};
    bool recoveryPlanned{false}; // 仅对 ConsumeResource 且 consumed==0 时有意义
    // 空字符串表示成功；非空表示失败原因（见 docs/architecture/foundation/telemetry-schema.md）。
    std::string failureReason;
};

struct AgentSnapshot {
    std::uint32_t entityId{0};
    std::string name;
    std::uint32_t mapId{1};
    Float2 position{}; // 地图内世界坐标（直线移动语义）
};

struct MovementSnapshot {
    std::uint32_t entityId{0};
    std::uint32_t mapId{1};
    Float2 position{};
    std::uint32_t targetMapId{1};
    Float2 target{};
    float speed{0.0f};
};

struct TickTelemetry {
    std::uint32_t schema_version{kTickTelemetrySchemaVersion};
    std::uint64_t step{0};
    float stepSeconds{0.0f};
    std::vector<ResourceSnapshot> resources;
    std::vector<NeedSnapshot> needs;
    std::vector<PlannerSnapshot> plannerDecisions;
    std::vector<ActionSnapshot> actions;
    std::vector<WorkshopAttemptSnapshot> workshopAttempts;
    std::vector<ResourceAttemptSnapshot> resourceAttempts;
    std::vector<AgentSnapshot> agents;
    std::vector<MovementSnapshot> movements;
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

