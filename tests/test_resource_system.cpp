#include <gtest/gtest.h>

#include <vector>

#include <entt/entt.hpp>

#include "genesis/messaging/EventBus.hpp"
#include "genesis/world/WorldRegistry.hpp"
#include "genesis/world/components/ResourceInventory.hpp"
#include "genesis/world/components/ResourceSpawn.hpp"
#include "genesis/world/events/ResourceEvents.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

using namespace genesis::world;

namespace {

LocationGraph makeGraphWithSpawn(std::uint32_t capacity, std::uint32_t rate) {
    LocationGraph graph;
    graph.nodes.push_back(LocationNode{LocationId{1}, InvalidLocation, "Root", LocationKind::Region, true});

    ResourceSpawn spawn{};
    spawn.name = "Test Spawn";
    spawn.type = ResourceType::Food;
    spawn.location = LocationId{1};
    spawn.capacity = capacity;
    spawn.ratePerStep = rate;
    graph.spawns.push_back(spawn);

    return graph;
}

} // namespace

TEST(ResourceSystemTest, ReplenishesInventory) {
    WorldRegistry world;
    world.setGraph(makeGraphWithSpawn(10, 3));

    entt::registry registry;
    genesis::messaging::EventBus bus;
    system::ResourceSystem resourceSystem(world, bus);
    resourceSystem.initialize(registry);

    auto view = registry.view<components::ResourceInventory>();
    std::size_t count = 0;
    entt::entity entity = entt::null;
    for (auto e : view) {
        entity = e;
        ++count;
    }

    ASSERT_EQ(count, 1U);
    auto& inventory = registry.get<components::ResourceInventory>(entity);
    EXPECT_EQ(inventory.current, inventory.capacity);

    inventory.current = 5;
    resourceSystem.tick(registry, 1);
    EXPECT_EQ(inventory.current, 8);

    resourceSystem.tick(registry, 2);
    EXPECT_EQ(inventory.current, 10);

    resourceSystem.tick(registry, 3);
    EXPECT_EQ(inventory.current, 10);
}

TEST(ResourceSystemTest, InitializeIsIdempotent) {
    WorldRegistry world;
    world.setGraph(makeGraphWithSpawn(5, 1));

    entt::registry registry;
    genesis::messaging::EventBus bus;
    system::ResourceSystem resourceSystem(world, bus);
    resourceSystem.initialize(registry);
    resourceSystem.initialize(registry);

    auto view = registry.view<components::ResourceInventory>();
    std::size_t count = 0;
    for ([[maybe_unused]] auto entity : view) {
        ++count;
    }

    EXPECT_EQ(count, 1U);
}

TEST(ResourceSystemTest, ConsumptionEmitsEventsAndHandlesPreferredLocation) {
    WorldRegistry world;
    LocationGraph graph;
    graph.nodes.push_back(LocationNode{LocationId{1}, InvalidLocation, "Tavern", LocationKind::Building, true});
    graph.nodes.push_back(LocationNode{LocationId{2}, InvalidLocation, "Bakery", LocationKind::Building, true});

    ResourceSpawn tavern{};
    tavern.name = "Tavern Food";
    tavern.type = ResourceType::Food;
    tavern.location = LocationId{1};
    tavern.capacity = 10;
    tavern.ratePerStep = 2;

    ResourceSpawn bakery{};
    bakery.name = "Bakery Bread";
    bakery.type = ResourceType::Food;
    bakery.location = LocationId{2};
    bakery.capacity = 5;
    bakery.ratePerStep = 0;

    graph.spawns.push_back(tavern);
    graph.spawns.push_back(bakery);

    world.setGraph(graph);

    entt::registry registry;
    genesis::messaging::EventBus bus;

    std::vector<events::ResourceConsumed> consumedEvents;
    std::vector<events::ResourceLowStock> lowStockEvents;

    struct Recorder {
        std::vector<events::ResourceConsumed>* consumed{};
        std::vector<events::ResourceLowStock>* low{};

        void onConsumed(const events::ResourceConsumed& event) {
            consumed->push_back(event);
        }

        void onLow(const events::ResourceLowStock& event) {
            low->push_back(event);
        }
    } recorder{&consumedEvents, &lowStockEvents};

    bus.raw().sink<events::ResourceConsumed>().connect<&Recorder::onConsumed>(&recorder);
    bus.raw().sink<events::ResourceLowStock>().connect<&Recorder::onLow>(&recorder);

    system::ResourceSystem resourceSystem(world, bus);
    resourceSystem.initialize(registry);

    // Reduce inventory to trigger low stock threshold on tavern
    auto view = registry.view<components::ResourceInventory, components::ResourceSpawn>();
    for (auto [entity, inventory, spawn] : view.each()) {
        if (spawn.name == "Tavern Food") {
            inventory.current = 3;
        }
        if (spawn.name == "Bakery Bread") {
            inventory.current = 2;
        }
    }

    const auto resultPreferred = resourceSystem.consume(registry, ResourceType::Food, 2, LocationId{1});
    EXPECT_EQ(resultPreferred, 2U);
    ASSERT_EQ(consumedEvents.size(), 1U);
    EXPECT_EQ(consumedEvents.front().name, "Tavern Food");
    EXPECT_EQ(consumedEvents.front().remaining, 1U);
    ASSERT_EQ(lowStockEvents.size(), 1U);
    EXPECT_EQ(lowStockEvents.front().name, "Tavern Food");

    consumedEvents.clear();
    lowStockEvents.clear();

    const auto resultFallback = resourceSystem.consume(registry, ResourceType::Food, 3, LocationId{3});
    EXPECT_EQ(resultFallback, 3U);
    ASSERT_EQ(consumedEvents.size(), 2U);
    EXPECT_EQ(consumedEvents.front().name, "Tavern Food");
    EXPECT_EQ(consumedEvents.back().name, "Bakery Bread");
}
