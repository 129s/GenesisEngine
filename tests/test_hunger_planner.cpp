#include <gtest/gtest.h>

#include <entt/entt.hpp>

#include "genesis/agents/AgentComponents.hpp"
#include "genesis/agents/NeedSystem.hpp"
#include "genesis/planner/HungerPlanner.hpp"
#include "genesis/world/WorldRegistry.hpp"
#include "genesis/world/system/ResourceSystem.hpp"
#include "genesis/world/components/ResourceInventory.hpp"
#include "genesis/world/components/ResourceSpawn.hpp"
#include "genesis/world/WorldTypes.hpp"
#include "genesis/messaging/EventBus.hpp"

using namespace genesis;

namespace {

world::LocationGraph makeGraph() {
    world::LocationGraph graph;
    graph.nodes.push_back({world::LocationId{1}, world::InvalidLocation, "Home", world::LocationKind::Building, true});
    graph.nodes.push_back({world::LocationId{2}, world::InvalidLocation, "Tavern", world::LocationKind::Building, true});
    graph.nodes.push_back({world::LocationId{3}, world::InvalidLocation, "Bakery", world::LocationKind::Building, true});

    graph.edges.push_back({world::LocationId{1}, world::LocationId{2}, 1.0f, true});
    graph.edges.push_back({world::LocationId{2}, world::LocationId{3}, 1.0f, true});

    world::ResourceSpawn tavern{};
    tavern.name = "Tavern";
    tavern.type = world::ResourceType::Food;
    tavern.location = world::LocationId{2};
    tavern.capacity = 5;
    tavern.ratePerStep = 0;

    world::ResourceSpawn bakery{};
    bakery.name = "Bakery";
    bakery.type = world::ResourceType::Food;
    bakery.location = world::LocationId{3};
    bakery.capacity = 5;
    bakery.ratePerStep = 0;

    graph.spawns.push_back(tavern);
    graph.spawns.push_back(bakery);

    return graph;
}

agents::NeedDescriptor hungerDescriptor() {
    agents::NeedDescriptor hunger{};
    hunger.type = agents::NeedType::Hunger;
    hunger.minValue = 0.0f;
    hunger.maxValue = 100.0f;
    hunger.decayPerSecond = 0.0f;
    hunger.satisfiedThreshold = 25.0f;
    hunger.criticalThreshold = 75.0f;
    return hunger;
}

} // namespace

TEST(HungerPlannerTest, ChoosesNearestAvailableSpawn) {
    world::WorldRegistry world;
    world.setGraph(makeGraph());

    entt::registry registry;
    messaging::EventBus bus;
    world::system::ResourceSystem resourceSystem(world, bus);
    resourceSystem.initialize(registry);

    auto view = registry.view<world::components::ResourceInventory, world::components::ResourceSpawn>();
    for (auto entity : view) {
        auto& inventory = view.get<world::components::ResourceInventory>(entity);
        const auto& spawn = view.get<world::components::ResourceSpawn>(entity);
        if (spawn.location == world::LocationId{2}) {
            inventory.current = 0;
        }
        if (spawn.location == world::LocationId{3}) {
            inventory.current = 4;
        }
    }

    agents::NeedSystem needSystem;
    needSystem.setDefaultDescriptor(hungerDescriptor());

    auto agent = registry.create();
    auto& needComponent = registry.emplace<agents::NeedComponent>(agent);
    needSystem.applyDefaults(needComponent);
    needComponent.needs.setState(agents::NeedType::Hunger, 90.0f);
    registry.emplace<agents::components::AgentLocation>(agent, agents::components::AgentLocation{world::LocationId{1}});

    planner::PlannerContext context{registry, world, resourceSystem};
    planner::HungerPlanner planner({});
    planner.evaluate(0, context);

    std::uint32_t bakeryStock = 0;
    for (auto entity : view) {
        const auto& inventory = view.get<world::components::ResourceInventory>(entity);
        const auto& spawn = view.get<world::components::ResourceSpawn>(entity);
        if (spawn.location == world::LocationId{3}) {
            bakeryStock = inventory.current;
        }
    }

    EXPECT_EQ(bakeryStock, 2U);
}

