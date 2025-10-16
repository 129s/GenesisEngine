#include <gtest/gtest.h>

#include <vector>

#include <entt/entt.hpp>

#include "genesis/agents/NeedSatisfier.hpp"
#include "genesis/agents/NeedSystem.hpp"
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

    NeedSatisfier satisfier{NeedSatisfierConfig{.hungerUnitsPerRequest = 2, .hungerReliefPerUnit = 10.0f}};
    satisfier.update(registry, resourceSystem);

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

    NeedSatisfier satisfier{NeedSatisfierConfig{.hungerUnitsPerRequest = 3, .hungerReliefPerUnit = 5.0f}};
    satisfier.update(registry, resourceSystem);

    auto* hungerState = needs.needs.state(NeedType::Hunger);
    ASSERT_NE(hungerState, nullptr);
    EXPECT_FLOAT_EQ(hungerState->value, 85.0f);

    auto view = registry.view<genesis::world::components::ResourceInventory>();
    for (auto [spawnEntity, inventory] : view.each()) {
        EXPECT_EQ(inventory.current, 0U);
    }
}

