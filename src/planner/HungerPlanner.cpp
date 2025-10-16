#include "genesis/planner/HungerPlanner.hpp"

#include <limits>
#include <queue>
#include <unordered_map>

#include <entt/entt.hpp>

#include "genesis/agents/AgentComponents.hpp"
#include "genesis/agents/NeedSystem.hpp"
#include "genesis/world/WorldRegistry.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

namespace genesis::planner {

namespace {

world::LocationId defaultHungerLocator(PlannerContext& context, entt::entity agent) {
    const auto* location = context.registry.try_get<genesis::agents::components::AgentLocation>(agent);
    if (!location || location->location == genesis::world::InvalidLocation) {
        return genesis::world::InvalidLocation;
    }

    std::unordered_map<genesis::world::LocationId, std::uint32_t, genesis::world::LocationIdHasher> available;
    auto resourceView = context.registry.view<genesis::world::components::ResourceInventory, genesis::world::components::ResourceSpawn>();
    for (auto entity : resourceView) {
        const auto& inventory = resourceView.get<genesis::world::components::ResourceInventory>(entity);
        const auto& spawn = resourceView.get<genesis::world::components::ResourceSpawn>(entity);
        if (inventory.current > 0) {
            available.emplace(spawn.location, inventory.current);
        }
    }

    if (available.empty()) {
        return genesis::world::InvalidLocation;
    }

    std::queue<std::pair<genesis::world::LocationId, std::uint32_t>> frontier;
    std::unordered_map<genesis::world::LocationId, std::uint32_t, genesis::world::LocationIdHasher> visited;

    frontier.emplace(location->location, 0);
    visited.emplace(location->location, 0);

    genesis::world::LocationId bestLocation = genesis::world::InvalidLocation;
    std::uint32_t bestDistance = std::numeric_limits<std::uint32_t>::max();

    while (!frontier.empty()) {
        const auto [current, distance] = frontier.front();
        frontier.pop();

        if (auto it = available.find(current); it != available.end()) {
            if (distance < bestDistance) {
                bestDistance = distance;
                bestLocation = current;
            }
            continue;
        }

        for (const auto& edge : context.world.edgesFrom(current)) {
            if (!visited.contains(edge.to)) {
                visited.emplace(edge.to, distance + 1);
                frontier.emplace(edge.to, distance + 1);
            }
        }
    }

    return bestLocation;
}

} // namespace

HungerPlanner::HungerPlanner(genesis::agents::NeedSatisfierConfig config, world::LocationId (*locator)(PlannerContext&, entt::entity))
    : m_locator(locator ? locator : defaultHungerLocator)
    , m_satisfier([&]() {
          config.hungerPreferredLocator = [this](entt::entity entity) {
              if (!m_currentContext) {
                  return genesis::world::InvalidLocation;
              }
              return m_locator(*m_currentContext, entity);
          };
          return genesis::agents::NeedSatisfier(config);
      }()) {
}

void HungerPlanner::evaluate(std::uint64_t /*step*/, PlannerContext& context) {
    m_currentContext = &context;
    m_satisfier.update(context.registry, context.resourceSystem);
    m_currentContext = nullptr;
}

} // namespace genesis::planner

