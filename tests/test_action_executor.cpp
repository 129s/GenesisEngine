#include <gtest/gtest.h>

#include <entt/entt.hpp>

#include "genesis/agents/ActionSystem.hpp"
#include "genesis/agents/AgentComponents.hpp"
#include "genesis/agents/NeedSystem.hpp"
#include "genesis/agents/NeedSatisfier.hpp"
#include "genesis/world/WorldRegistry.hpp"
#include "genesis/world/system/ResourceSystem.hpp"
#include "genesis/world/components/ResourceInventory.hpp"
#include "genesis/world/components/ResourceSpawn.hpp"
#include "genesis/world/WorldTypes.hpp"
#include "genesis/messaging/EventBus.hpp"

using genesis::agents::ActionExecutor;
using genesis::agents::ActionQueue;
using genesis::agents::ActionType;
using genesis::agents::components::AgentLocation;
using genesis::agents::components::MovementIntent;
using genesis::agents::NeedComponent;
using genesis::agents::NeedSystem;
using genesis::agents::NeedType;
using genesis::world::LocationGraph;
using genesis::world::LocationId;
using genesis::world::LocationKind;
using genesis::world::LocationNode;
using genesis::world::ResourceSpawn;
using genesis::world::ResourceType;

namespace {

LocationGraph createGraph() {
    LocationGraph graph;
    graph.nodes.push_back(LocationNode{LocationId{1}, genesis::world::InvalidLocation, "Kitchen", LocationKind::Building, true});
    graph.nodes.push_back(LocationNode{LocationId{2}, genesis::world::InvalidLocation, "Dorm", LocationKind::Building, true});
    graph.edges.push_back(genesis::world::PathEdge{LocationId{2}, LocationId{1}, 1.0f, true});

    ResourceSpawn spawn{};
    spawn.name = "Kitchen";
    spawn.type = ResourceType::Food;
    spawn.location = LocationId{1};
    spawn.capacity = 10;
    spawn.ratePerStep = 0;
    graph.spawns.push_back(spawn);

    return graph;
}

void configureNeeds(NeedSystem& system, NeedComponent& component) {
    genesis::agents::NeedDescriptor hunger{};
    hunger.type = NeedType::Hunger;
    hunger.minValue = 0.0f;
    hunger.maxValue = 100.0f;
    hunger.decayPerSecond = 0.0f;
    hunger.satisfiedThreshold = 20.0f;
    hunger.criticalThreshold = 75.0f;
    system.setDefaultDescriptor(hunger);
    system.applyDefaults(component);
}

} // namespace

TEST(ActionExecutorTest, ConsumesResourceWhenAgentAtTarget) {
    LocationGraph graph = createGraph();
    genesis::world::WorldRegistry world;
    world.setGraph(graph);

    entt::registry registry;
    genesis::messaging::EventBus bus;
    genesis::world::system::ResourceSystem resourceSystem(world, bus);
    resourceSystem.initialize(registry);

    NeedSystem needSystem;

    auto entity = registry.create();
    auto& needs = registry.emplace<NeedComponent>(entity);
    configureNeeds(needSystem, needs);
    needs.needs.setState(NeedType::Hunger, 90.0f);

    auto& location = registry.emplace<AgentLocation>(entity);
    location.location = LocationId{1};

    ActionExecutor executor(world, resourceSystem);
    executor.requestConsume(entity, LocationId{1}, ResourceType::Food, 2, 10.0f, registry);
    executor.update(registry, 0.1f);

    auto* queue = registry.try_get<ActionQueue>(entity);
    EXPECT_TRUE(queue == nullptr || queue->tasks.empty());

    auto* intent = registry.try_get<MovementIntent>(entity);
    EXPECT_EQ(intent, nullptr);

    auto* hungerState = needs.needs.state(NeedType::Hunger);
    ASSERT_NE(hungerState, nullptr);
    EXPECT_FLOAT_EQ(hungerState->value, 70.0f);

    auto resourceView = registry.view<genesis::world::components::ResourceInventory, genesis::world::components::ResourceSpawn>();
    std::uint32_t stock = 0;
    for (auto [spawnEntity, inventory, spawn] : resourceView.each()) {
        if (spawn.location == LocationId{1}) {
            stock = inventory.current;
        }
    }
    EXPECT_EQ(stock, 8U);
}

TEST(ActionExecutorTest, IssuesMovementWhenAgentAwayFromTarget) {
    LocationGraph graph = createGraph();
    genesis::world::WorldRegistry world;
    world.setGraph(graph);

    entt::registry registry;
    genesis::messaging::EventBus bus;
    genesis::world::system::ResourceSystem resourceSystem(world, bus);
    resourceSystem.initialize(registry);

    NeedSystem needSystem;

    auto entity = registry.create();
    auto& needs = registry.emplace<NeedComponent>(entity);
    configureNeeds(needSystem, needs);
    needs.needs.setState(NeedType::Hunger, 90.0f);

    auto& location = registry.emplace<AgentLocation>(entity);
    location.location = LocationId{2};

    ActionExecutor executor(world, resourceSystem);
    executor.requestConsume(entity, LocationId{1}, ResourceType::Food, 2, 10.0f, registry);

    auto* queue = registry.try_get<ActionQueue>(entity);
    ASSERT_NE(queue, nullptr);
    ASSERT_FALSE(queue->tasks.empty());
    EXPECT_EQ(queue->tasks.front().type, ActionType::MoveTo);
    EXPECT_EQ(queue->tasks.front().location, LocationId{1});

    executor.update(registry, 0.1f);

    auto* intent = registry.try_get<MovementIntent>(entity);
    ASSERT_NE(intent, nullptr);
    EXPECT_EQ(intent->target, LocationId{1});
    EXPECT_FALSE(queue->tasks.empty());
    EXPECT_EQ(queue->tasks.front().type, ActionType::MoveTo);
}

