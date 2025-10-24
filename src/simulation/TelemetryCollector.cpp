#include "genesis/simulation/TelemetryCollector.hpp"

#include "genesis/agents/Movement2D.hpp"

namespace genesis::simulation {

telemetry::TickTelemetry TelemetryCollector::collect(entt::registry& registry, std::uint64_t stepIndex, float stepSeconds) const {
    telemetry::TickTelemetry tick{};
    tick.step = stepIndex;
    tick.stepSeconds = stepSeconds;

    auto agentView = registry.view<genesis::agents::components::AgentLocation2D>();
    agentView.each([&](auto entity, const genesis::agents::components::AgentLocation2D& loc) {
        telemetry::AgentSnapshot snapshot{};
        snapshot.entityId = static_cast<std::uint32_t>(entt::to_integral(entity));
        snapshot.name = "Agent";
        snapshot.mapId = loc.mapId;
        snapshot.position.x = loc.x;
        snapshot.position.y = loc.y;
        tick.agents.push_back(std::move(snapshot));
    });

    return tick;
}

} // namespace genesis::simulation

