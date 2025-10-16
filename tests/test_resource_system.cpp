#include <gtest/gtest.h>

#include <entt/entt.hpp>

#include "genesis/world/WorldRegistry.hpp"
#include "genesis/world/system/ResourceSystem.hpp"
#include "genesis/world/components/ResourceInventory.hpp"
#include "genesis/world/components/ResourceSpawn.hpp"

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
    system::ResourceSystem resourceSystem(world);
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
    system::ResourceSystem resourceSystem(world);
    resourceSystem.initialize(registry);
    resourceSystem.initialize(registry);

    auto view = registry.view<components::ResourceInventory>();
    std::size_t count = 0;
    for ([[maybe_unused]] auto entity : view) {
        ++count;
    }

    EXPECT_EQ(count, 1U);
}
