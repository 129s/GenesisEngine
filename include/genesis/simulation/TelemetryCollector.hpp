#pragma once

#include <cstdint>
#include <entt/entt.hpp>
#include <vector>

#include "genesis/telemetry/TelemetryBuffer.hpp"
namespace genesis { namespace world { class WorldDatabase; } }
namespace genesis { namespace world { namespace system { class ResourceSystem; } } }

namespace genesis::simulation {

class TelemetryCollector {
public:
    telemetry::TickTelemetry collect(entt::registry& registry,
                                     const genesis::world::WorldDatabase* db,
                                     const std::vector<telemetry::ResourceSnapshot>& resources,
                                     std::uint64_t stepIndex,
                                     float stepSeconds) const;
};

} // namespace genesis::simulation
