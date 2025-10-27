#include "genesis/simulation/TelemetryCollector.hpp"

#include "genesis/world/WorldDatabase.hpp"

namespace genesis::simulation {

telemetry::TickTelemetry TelemetryCollector::collect(std::span<const telemetry::AgentSnapshot> agents,
                                                     const genesis::world::WorldDatabase* db,
                                                     std::span<const telemetry::ResourceSnapshot> resources,
                                                     std::uint64_t stepIndex,
                                                     float stepSeconds) const {
    telemetry::TickTelemetry tick{};
    tick.step = stepIndex;
    tick.stepSeconds = stepSeconds;

    if (!agents.empty()) {
        tick.agents.reserve(agents.size());
        for (const auto& agent : agents) {
            tick.agents.push_back(agent);
        }
    }

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
