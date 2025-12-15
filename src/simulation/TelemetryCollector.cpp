#include "genesis/simulation/TelemetryCollector.hpp"

#include "genesis/world/WorldDatabase.hpp"

namespace genesis::simulation {

telemetry::TickTelemetry TelemetryCollector::collect(std::span<const telemetry::AgentSnapshot> agents,
                                                     const genesis::world::WorldDatabase* db,
                                                     std::span<const telemetry::ResourceSnapshot> resources,
                                                     std::span<const telemetry::NeedSnapshot> needs,
                                                     std::span<const telemetry::PlannerSnapshot> plannerDecisions,
                                                     std::span<const telemetry::ActionSnapshot> actions,
                                                     std::span<const telemetry::WorkshopAttemptSnapshot> workshopAttempts,
                                                     std::uint64_t stepIndex,
                                                     float stepSeconds) const {
    telemetry::TickTelemetry tick{};
    tick.schema_version = 4;
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

    if (!needs.empty()) {
        tick.needs.reserve(needs.size());
        for (const auto& need : needs) {
            tick.needs.push_back(need);
        }
    }

    if (!plannerDecisions.empty()) {
        tick.plannerDecisions.reserve(plannerDecisions.size());
        for (const auto& decision : plannerDecisions) {
            tick.plannerDecisions.push_back(decision);
        }
    }

    if (!actions.empty()) {
        tick.actions.reserve(actions.size());
        for (const auto& action : actions) {
            tick.actions.push_back(action);
        }
    }

    if (!workshopAttempts.empty()) {
        tick.workshopAttempts.reserve(workshopAttempts.size());
        for (const auto& attempt : workshopAttempts) {
            tick.workshopAttempts.push_back(attempt);
        }
    }

    return tick;
}

} // namespace genesis::simulation
