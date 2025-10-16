#include "genesis/planner/HungerPlanner.hpp"

#include <limits>
#include <queue>
#include <unordered_map>

#include <entt/entt.hpp>

#include "genesis/agents/AgentComponents.hpp"
#include "genesis/agents/NeedSystem.hpp"
#include "genesis/world/WorldRegistry.hpp"
#include "genesis/world/components/ResourceInventory.hpp"
#include "genesis/world/components/ResourceSpawn.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

namespace genesis::planner {

namespace {

LocationChoice defaultHungerLocator(PlannerContext& context, entt::entity agent) {
    const auto* location = context.registry.try_get<genesis::agents::components::AgentLocation>(agent);
    if (!location || location->location == genesis::world::InvalidLocation) {
        return {genesis::world::InvalidLocation, 0.0f, 0.0f};
    }

    std::unordered_map<genesis::world::LocationId, std::uint32_t, genesis::world::LocationIdHasher> available;
    auto resourceView = context.registry.view<genesis::world::components::ResourceInventory, genesis::world::components::ResourceSpawn>();
    for (auto entity : resourceView) {
        const auto& inventory = resourceView.get<genesis::world::components::ResourceInventory>(entity);
        const auto& spawn = resourceView.get<genesis::world::components::ResourceSpawn>(entity);
        available[spawn.location] = inventory.current;
    }

    if (available.empty()) {
        return {genesis::world::InvalidLocation, 0.0f, 0.0f};
    }

    std::unordered_map<genesis::world::LocationId, std::uint32_t, genesis::world::LocationIdHasher> occupancy;
    auto agentView = context.registry.view<genesis::agents::components::AgentLocation>();
    for (auto [entityId, agentLoc] : agentView.each()) {
        ++occupancy[agentLoc.location];
    }

    auto occupancyPenalty = [&](genesis::world::LocationId loc) -> float {
        auto it = occupancy.find(loc);
        if (it == occupancy.end()) {
            return 0.0f;
        }
        return static_cast<float>(it->second) * 0.75f;
    };

    auto stockPenalty = [&](genesis::world::LocationId loc) -> float {
        auto it = available.find(loc);
        if (it == available.end()) {
            return 1000.0f;
        }
        if (it->second == 0) {
            return 1000.0f;
        }
        if (it->second <= 1) {
            return 3.0f;
        }
        return 0.0f;
    };

    using Node = std::pair<float, genesis::world::LocationId>;
    auto cmp = [](const Node& lhs, const Node& rhs) { return lhs.first > rhs.first; };
    std::priority_queue<Node, std::vector<Node>, decltype(cmp)> frontier(cmp);

    std::unordered_map<genesis::world::LocationId, float, genesis::world::LocationIdHasher> bestCost;
    frontier.emplace(0.0f, location->location);
    bestCost[location->location] = 0.0f;

    genesis::world::LocationId bestLocation = genesis::world::InvalidLocation;
    float bestScore = std::numeric_limits<float>::max();
    float bestTravelCost = 0.0f;

    while (!frontier.empty()) {
        const auto [pathCost, current] = frontier.top();
        frontier.pop();

        if (pathCost > bestCost[current] + 1e-4f) {
            continue;
        }

        if (auto it = available.find(current); it != available.end() && it->second > 0) {
            const float score = pathCost + occupancyPenalty(current) + stockPenalty(current);
            if (score < bestScore) {
                bestScore = score;
                bestTravelCost = pathCost;
                bestLocation = current;
            }
        }

        for (const auto& edge : context.world.edgesFrom(current)) {
            const float nextCost = pathCost + edge.cost;
            auto [it, inserted] = bestCost.emplace(edge.to, nextCost);
            if (!inserted) {
                if (nextCost + 1e-4f < it->second) {
                    it->second = nextCost;
                } else {
                    continue;
                }
            }
            frontier.emplace(nextCost, edge.to);
        }
    }

    if (bestLocation == genesis::world::InvalidLocation) {
        genesis::world::LocationId fallback = genesis::world::InvalidLocation;
        std::uint32_t bestStock = 0;
        float fallbackCost = 0.0f;
        for (const auto& [loc, stock] : available) {
            if (stock > bestStock) {
                bestStock = stock;
                fallback = loc;
                if (auto it = bestCost.find(loc); it != bestCost.end()) {
                    fallbackCost = it->second;
                }
            }
        }
        return {fallback, fallbackCost, fallbackCost + occupancyPenalty(fallback) + stockPenalty(fallback)};
    }

    return {bestLocation, bestTravelCost, bestScore};
}
} // namespace

HungerPlanner::HungerPlanner(genesis::agents::NeedSatisfierConfig config, LocationChoice (*locator)(PlannerContext&, entt::entity))
    : m_locator(locator ? locator : defaultHungerLocator)
    , m_satisfier([&]() {
          config.hungerPreferredLocator = [this](entt::entity entity) {
              if (!m_currentContext) {
                  return genesis::world::InvalidLocation;
              }
              const auto choice = m_locator(*m_currentContext, entity);
              if (m_decisions) {
                  m_decisions->push_back(HungerDecision{
                      .agent = entity,
                      .target = choice.target,
                      .travelCost = choice.travelCost,
                      .score = choice.score,
                  });
              }
              return choice.target;
          };
          return genesis::agents::NeedSatisfier(config);
      }()) {
}

void HungerPlanner::evaluate(std::uint64_t /*step*/, PlannerContext& context) {
    m_currentContext = &context;
    m_decisions = context.hungerDecisions;
    m_satisfier.update(context.registry, context.resourceSystem, context.actionExecutor);
    m_decisions = nullptr;
    m_currentContext = nullptr;
}

} // namespace genesis::planner





