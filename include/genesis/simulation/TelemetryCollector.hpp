#pragma once

#include <cstdint>
#include <entt/entt.hpp>
#include <memory>

#include "genesis/telemetry/TelemetryBuffer.hpp"
namespace genesis { namespace world { class WorldDatabase; } }
namespace genesis { namespace simulation { class ResourceSystem2D; } }

namespace genesis::simulation {

class TelemetryCollector {
public:
    telemetry::TickTelemetry collect(entt::registry& registry,
                                     const genesis::world::WorldDatabase* db,
                                     const genesis::simulation::ResourceSystem2D* resources,
                                     std::uint64_t stepIndex,
                                     float stepSeconds) const;
};

} // namespace genesis::simulation
