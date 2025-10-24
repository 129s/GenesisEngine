#include "genesis/simulation/TelemetryCollector.hpp"

#include "genesis/agents/Movement2D.hpp"
#include "genesis/world/WorldDatabase.hpp"
#include "genesis/simulation/ResourceSystem2D.hpp"

namespace genesis::simulation {

telemetry::TickTelemetry TelemetryCollector::collect(entt::registry& registry,
                                                     const genesis::world::WorldDatabase* db,
                                                     const genesis::simulation::ResourceSystem2D* resources,
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
    if (db && resources) {
        for (const auto& [iid, state] : resources->states()) {
            telemetry::ResourceV2Snapshot r{};
            r.interactionId = iid;
            r.current = state.current;
            r.capacity = state.capacity;
            if (auto inter = db->findInteraction(iid)) {
                r.name = inter->name;
                r.mapId = inter->mapId;
            }
            tick.resourcesV2.push_back(std::move(r));
        }
    }

    return tick;
}

} // namespace genesis::simulation
