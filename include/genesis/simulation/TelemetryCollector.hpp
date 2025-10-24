#pragma once

#include <cstdint>
#include <entt/entt.hpp>

#include "genesis/telemetry/TelemetryBuffer.hpp"

namespace genesis::simulation {

class TelemetryCollector {
public:
    telemetry::TickTelemetry collect(entt::registry& registry, std::uint64_t stepIndex, float stepSeconds) const;
};

} // namespace genesis::simulation

