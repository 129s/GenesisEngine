#include "genesis/simulation/TelemetryCollector.hpp"

#include "genesis/agents/Movement2D.hpp"
#include "genesis/world/WorldDatabase.hpp"

namespace genesis::simulation {

telemetry::TickTelemetry TelemetryCollector::collect(entt::registry& registry,
                                                     const genesis::world::WorldDatabase* db,
                                                     const std::vector<telemetry::ResourceSnapshot>& resources,
                                                     std::uint64_t stepIndex,
                                                     float stepSeconds) const {
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

    // v2 资源采集
    if (!resources.empty()) {
        tick.resources.reserve(resources.size());
        for (auto resource : resources) {
            if (db && resource.mapId == 0) {
                if (auto inter = db->findInteraction(resource.interactionId)) {
                    resource.name = inter->name;
                    resource.mapId = inter->mapId;
                }
            }
            tick.resources.push_back(std::move(resource));
        }
    }

    return tick;
}

} // namespace genesis::simulation
