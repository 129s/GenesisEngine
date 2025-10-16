#include <gtest/gtest.h>

#include <entt/entt.hpp>

#include "genesis/agents/AgentComponents.hpp"
#include "genesis/agents/NeedSystem.hpp"
#include "genesis/planner/HungerPlanner.hpp"
#include "genesis/world/WorldRegistry.hpp"
#include "genesis/world/WorldTypes.hpp"
#include "genesis/world/components/ResourceInventory.hpp"
#include "genesis/world/components/ResourceSpawn.hpp"
#include "genesis/world/system/ResourceSystem.hpp"
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

    graph.spawns.push_back({"Tavern", world::ResourceType::Food, world::LocationId{2}, 5, 0});
    graph.spawns.push_back({"Bakery", world::ResourceType::Food, world::LocationId{3}, 5, 0});

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

void configureAgent(entt::registry& registry, agents::NeedSystem& needs, entt::entity agent, world::LocationId loc) {
    auto& component = registry.emplace<agents::NeedComponent>(agent);
    needs.applyDefaults(component);
    component.needs.setState(agents::NeedType::Hunger, 90.0f);
    registry.emplace<agents::components::AgentLocation>(agent, agents::components::AgentLocation{loc});
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
        } else {
            inventory.current = 4;
        }
    }

    agents::NeedSystem needSystem;
    needSystem.setDefaultDescriptor(hungerDescriptor());

    auto agent = registry.create();
    configureAgent(registry, needSystem, agent, world::LocationId{1});

    planner::PlannerContext context{registry, world, resourceSystem};
    std::vector<planner::HungerDecision> decisions;
    context.hungerDecisions = &decisions;
    planner::HungerPlanner planner({.hungerUnitsPerRequest = 2, .hungerReliefPerUnit = 10.0f});
    planner.evaluate(0, context);

    ASSERT_EQ(decisions.size(), 1U);
    EXPECT_EQ(decisions.front().target, world::LocationId{3});
    EXPECT_GT(decisions.front().travelCost, 0.0f);

    auto* intent = registry.try_get<agents::components::MovementIntent>(agent);
    ASSERT_NE(intent, nullptr);
    EXPECT_EQ(intent->target, world::LocationId{3});

    std::uint32_t bakerStock = 0;
    for (auto entity : view) {
        const auto& inventory = view.get<world::components::ResourceInventory>(entity);
        const auto& spawn = view.get<world::components::ResourceSpawn>(entity);
        if (spawn.location == world::LocationId{3}) {
            bakerStock = inventory.current;
        }
    }

    EXPECT_EQ(bakerStock, 4U);
}

TEST(HungerPlannerTest, PrefersLessCongestedEvenIfFarther) {
    world::WorldRegistry world;
    world.setGraph(makeGraph());

    entt::registry registry;
    messaging::EventBus bus;
    world::system::ResourceSystem resourceSystem(world, bus);
    resourceSystem.initialize(registry);

    auto view = registry.view<world::components::ResourceInventory, world::components::ResourceSpawn>();
    for (auto entity : view) {
        auto& inventory = view.get<world::components::ResourceInventory>(entity);
        inventory.current = 4;
    }

    // Add congestion at tavern (location 2).
    for (int i = 0; i < 5; ++i) {
        auto npc = registry.create();
        registry.emplace<agents::components::AgentLocation>(npc, agents::components::AgentLocation{world::LocationId{2}});
    }

    agents::NeedSystem needSystem;
    needSystem.setDefaultDescriptor(hungerDescriptor());

    auto agent = registry.create();
    configureAgent(registry, needSystem, agent, world::LocationId{1});

    planner::PlannerContext context{registry, world, resourceSystem};
    std::vector<planner::HungerDecision> decisions;
    context.hungerDecisions = &decisions;
    planner::HungerPlanner planner({.hungerUnitsPerRequest = 2, .hungerReliefPerUnit = 10.0f});
    planner.evaluate(0, context);

    ASSERT_EQ(decisions.size(), 1U);
    EXPECT_EQ(decisions.front().target, world::LocationId{3});
    EXPECT_GT(decisions.front().travelCost, 0.0f);

    auto* intent = registry.try_get<agents::components::MovementIntent>(agent);
    ASSERT_NE(intent, nullptr);
    EXPECT_EQ(intent->target, world::LocationId{3});

    std::uint32_t tavernStock = 0;
    std::uint32_t bakeryStock = 0;
    for (auto entity : view) {
        const auto& inventory = view.get<world::components::ResourceInventory>(entity);
        const auto& spawn = view.get<world::components::ResourceSpawn>(entity);
        if (spawn.location == world::LocationId{2}) {
            tavernStock = inventory.current;
        } else if (spawn.location == world::LocationId{3}) {
            bakeryStock = inventory.current;
        }
    }

    EXPECT_EQ(tavernStock, 4U);
    EXPECT_EQ(bakeryStock, 4U);
}

