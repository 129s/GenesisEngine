#pragma once

#include <cstdint>
#include <span>

#include "genesis/telemetry/TelemetryBuffer.hpp"
namespace genesis { namespace world { class WorldDatabase; } }

namespace genesis::simulation {

class TelemetryCollector {
public:
    telemetry::TickTelemetry collect(std::span<const telemetry::AgentSnapshot> agents,
                                     const genesis::world::WorldDatabase* db,
                                     std::span<const telemetry::ResourceSnapshot> resources,
                                     std::uint64_t stepIndex,
                                     float stepSeconds) const;
};

} // namespace genesis::simulation
