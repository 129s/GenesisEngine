#include <gtest/gtest.h>

#include <vector>

#include <entt/entt.hpp>

#include "genesis/agents/NeedSatisfier.hpp"
#include "genesis/agents/NeedSystem.hpp"
#include "genesis/agents/AgentComponents.hpp"
#include "genesis/agents/ActionSystem.hpp"
#include "genesis/world/WorldRegistry.hpp"
#include "genesis/world/WorldTypes.hpp"
#include "genesis/world/system/ResourceSystem.hpp"
#include "genesis/world/components/ResourceInventory.hpp"
#include "genesis/world/components/ResourceSpawn.hpp"
#include "genesis/messaging/EventBus.hpp"

using genesis::agents::NeedComponent;
using genesis::agents::NeedDescriptor;
using genesis::agents::NeedSatisfier;
using genesis::agents::NeedSatisfierConfig;
using genesis::agents::NeedSystem;
using genesis::agents::NeedType;
using genesis::world::LocationGraph;
using genesis::world::LocationId;
using genesis::world::LocationKind;
using genesis::world::LocationNode;
using genesis::world::ResourceSpawn;
using genesis::world::ResourceType;

namespace {

LocationGraph createSingleSpawnGraph(std::uint32_t capacity) {
    LocationGraph graph;
    graph.nodes.push_back(LocationNode{LocationId{1}, genesis::world::InvalidLocation, "Test", LocationKind::Building, true});

    ResourceSpawn spawn{};
    spawn.name = "Test Kitchen";
    spawn.type = ResourceType::Food;
    spawn.location = LocationId{1};
    spawn.capacity = capacity;
    spawn.ratePerStep = 0;

    graph.spawns.push_back(spawn);
    return graph;
}

NeedDescriptor hungerDescriptor() {
    NeedDescriptor hunger{};
    hunger.type = NeedType::Hunger;
    hunger.minValue = 0.0f;
    hunger.maxValue = 100.0f;
    hunger.decayPerSecond = 0.0f;
    hunger.satisfiedThreshold = 25.0f;
    hunger.criticalThreshold = 75.0f;
    return hunger;
}

} // namespace

TEST(NeedSatisfierTest, ConsumesFoodAndReducesHunger) {
    LocationGraph graph = createSingleSpawnGraph(10);
    genesis::world::WorldRegistry world;
    world.setGraph(graph);

    entt::registry registry;
    genesis::messaging::EventBus bus;
    genesis::world::system::ResourceSystem resourceSystem(world, bus);
    resourceSystem.initialize(registry);

    NeedSystem needSystem;
    needSystem.setDefaultDescriptor(hungerDescriptor());

    auto entity = registry.create();
    auto& needs = registry.emplace<NeedComponent>(entity);
    needSystem.applyDefaults(needs);
    needs.needs.setState(NeedType::Hunger, 90.0f);

    auto& location = registry.emplace<genesis::agents::components::AgentLocation>(entity);
    location.location = LocationId{1};

    genesis::agents::ActionExecutor executor(world, resourceSystem);

    NeedSatisfierConfig config{};
    config.hungerUnitsPerRequest = 2;
    config.hungerReliefPerUnit = 10.0f;
    config.hungerPreferredLocator = [](entt::entity) { return LocationId{1}; };
    NeedSatisfier satisfier{config};
    satisfier.update(registry, resourceSystem, &executor);
    executor.update(registry, 0.1f);

    auto view = registry.view<genesis::world::components::ResourceInventory, genesis::world::components::ResourceSpawn>();
    std::uint32_t currentStock = 0;
    for (auto [spawnEntity, inventory, spawn] : view.each()) {
        if (spawn.name == "Test Kitchen") {
            currentStock = inventory.current;
        }
    }

    EXPECT_EQ(currentStock, 8U);

    auto* hungerState = needs.needs.state(NeedType::Hunger);
    ASSERT_NE(hungerState, nullptr);
    EXPECT_FLOAT_EQ(hungerState->value, 70.0f);
}

TEST(NeedSatisfierTest, HandlesPartialConsumptionWhenStockLow) {
    LocationGraph graph = createSingleSpawnGraph(1);
    genesis::world::WorldRegistry world;
    world.setGraph(graph);

    entt::registry registry;
    genesis::messaging::EventBus bus;
    genesis::world::system::ResourceSystem resourceSystem(world, bus);
    resourceSystem.initialize(registry);

    NeedSystem needSystem;
    needSystem.setDefaultDescriptor(hungerDescriptor());

    auto entity = registry.create();
    auto& needs = registry.emplace<NeedComponent>(entity);
    needSystem.applyDefaults(needs);
    needs.needs.setState(NeedType::Hunger, 90.0f);

    auto& location = registry.emplace<genesis::agents::components::AgentLocation>(entity);
    location.location = LocationId{1};

    genesis::agents::ActionExecutor executor(world, resourceSystem);

    NeedSatisfierConfig config{};
    config.hungerUnitsPerRequest = 3;
    config.hungerReliefPerUnit = 5.0f;
    config.hungerPreferredLocator = [](entt::entity) { return LocationId{1}; };
    NeedSatisfier satisfier{config};
    satisfier.update(registry, resourceSystem, &executor);
    executor.update(registry, 0.1f);

    auto* hungerState = needs.needs.state(NeedType::Hunger);
    ASSERT_NE(hungerState, nullptr);
    EXPECT_FLOAT_EQ(hungerState->value, 85.0f);

    auto view = registry.view<genesis::world::components::ResourceInventory>();
    for (auto [spawnEntity, inventory] : view.each()) {
        EXPECT_EQ(inventory.current, 0U);
    }
}


TEST(NeedSatisfierTest, FallsBackWhenPreferredEmpty) {
    LocationGraph graph;
    graph.nodes.push_back(LocationNode{LocationId{1}, genesis::world::InvalidLocation, "Home", LocationKind::Building, true});
    graph.nodes.push_back(LocationNode{LocationId{2}, genesis::world::InvalidLocation, "Bakery", LocationKind::Building, true});

    ResourceSpawn emptySpawn{};
    emptySpawn.name = "Empty Home";
    emptySpawn.type = ResourceType::Food;
    emptySpawn.location = LocationId{1};
    emptySpawn.capacity = 0;
    emptySpawn.ratePerStep = 0;

    ResourceSpawn fullSpawn{};
    fullSpawn.name = "Bakery";
    fullSpawn.type = ResourceType::Food;
    fullSpawn.location = LocationId{2};
    fullSpawn.capacity = 5;
    fullSpawn.ratePerStep = 0;

    graph.spawns.push_back(emptySpawn);
    graph.spawns.push_back(fullSpawn);

    genesis::world::WorldRegistry world;
    world.setGraph(graph);

    entt::registry registry;
    genesis::messaging::EventBus bus;
    genesis::world::system::ResourceSystem resourceSystem(world, bus);
    resourceSystem.initialize(registry);

    // Ensure home spawn remains empty while bakery has resources.
    auto resourceView = registry.view<genesis::world::components::ResourceInventory, genesis::world::components::ResourceSpawn>();
    for (auto entity : resourceView) {
        auto& inventory = resourceView.get<genesis::world::components::ResourceInventory>(entity);
        const auto& spawn = resourceView.get<genesis::world::components::ResourceSpawn>(entity);
        if (spawn.name == "Empty Home") {
            inventory.current = 0;
        }
        if (spawn.name == "Bakery") {
            inventory.current = 4;
        }
    }

    NeedSystem needSystem;
    needSystem.setDefaultDescriptor(hungerDescriptor());

    auto agent = registry.create();
    auto& needs = registry.emplace<NeedComponent>(agent);
    needSystem.applyDefaults(needs);
    needs.needs.setState(NeedType::Hunger, 90.0f);

    auto& location = registry.emplace<genesis::agents::components::AgentLocation>(agent);
    location.location = LocationId{1};

    genesis::agents::ActionExecutor executor(world, resourceSystem);

    NeedSatisfierConfig config{};
    config.hungerUnitsPerRequest = 2;
    config.hungerReliefPerUnit = 8.0f;
    config.hungerPreferredLocator = [](entt::entity) { return LocationId{1}; };
    NeedSatisfier satisfier{config};

    satisfier.update(registry, resourceSystem, &executor);
    executor.update(registry, 0.1f);

    std::uint32_t homeStock = 0;
    std::uint32_t bakeryStock = 0;
    for (auto entity : resourceView) {
        const auto& inventory = resourceView.get<genesis::world::components::ResourceInventory>(entity);
        const auto& spawn = resourceView.get<genesis::world::components::ResourceSpawn>(entity);
        if (spawn.name == "Empty Home") {
            homeStock = inventory.current;
        }
        if (spawn.name == "Bakery") {
            bakeryStock = inventory.current;
        }
    }

    EXPECT_EQ(homeStock, 0U);
    EXPECT_EQ(bakeryStock, 2U);
}

TEST(NeedSatisfierTest, RequestsMovementWhenAwayFromPreferred) {
    using genesis::agents::components::AgentLocation;

    LocationGraph graph;
    graph.nodes.push_back(LocationNode{LocationId{1}, genesis::world::InvalidLocation, "Kitchen", LocationKind::Room, true});
    graph.nodes.push_back(LocationNode{LocationId{2}, genesis::world::InvalidLocation, "Dorm", LocationKind::Room, true});
    graph.edges.push_back(genesis::world::PathEdge{LocationId{2}, LocationId{1}, 1.0f, true});

    ResourceSpawn spawn{};
    spawn.name = "Kitchen";
    spawn.type = ResourceType::Food;
    spawn.location = LocationId{1};
    spawn.capacity = 5;
    spawn.ratePerStep = 0;
    graph.spawns.push_back(spawn);

    genesis::world::WorldRegistry world;
    world.setGraph(graph);

    entt::registry registry;
    genesis::messaging::EventBus bus;
    genesis::world::system::ResourceSystem resourceSystem(world, bus);
    resourceSystem.initialize(registry);

    NeedSystem needSystem;
    needSystem.setDefaultDescriptor(hungerDescriptor());

    auto entity = registry.create();
    auto& needs = registry.emplace<NeedComponent>(entity);
    needSystem.applyDefaults(needs);
    needs.needs.setState(NeedType::Hunger, 90.0f);

    auto& agentLocation = registry.emplace<AgentLocation>(entity);
    agentLocation.location = LocationId{2};

    genesis::agents::ActionExecutor executor(world, resourceSystem);

    NeedSatisfierConfig config{};
    config.hungerUnitsPerRequest = 2;
    config.hungerReliefPerUnit = 10.0f;
    config.hungerPreferredLocator = [](entt::entity) { return LocationId{1}; };
    NeedSatisfier satisfier{config};
    satisfier.update(registry, resourceSystem, &executor);

    auto* queue = registry.try_get<genesis::agents::ActionQueue>(entity);
    ASSERT_NE(queue, nullptr);
    ASSERT_FALSE(queue->tasks.empty());
    EXPECT_EQ(queue->tasks.front().type, genesis::agents::ActionType::MoveTo);

    executor.update(registry, 0.1f);

    auto* intent = registry.try_get<genesis::agents::components::MovementIntent>(entity);
    ASSERT_NE(intent, nullptr);
    EXPECT_EQ(intent->target.value, 1U);

    auto* hungerState = needs.needs.state(NeedType::Hunger);
    ASSERT_NE(hungerState, nullptr);
    EXPECT_FLOAT_EQ(hungerState->value, 90.0f);

    auto view = registry.view<genesis::world::components::ResourceInventory>();
    for (auto [spawnEntity, inventory] : view.each()) {
        EXPECT_EQ(inventory.current, 5U);
    }
}
