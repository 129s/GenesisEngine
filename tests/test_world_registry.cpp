#include <gtest/gtest.h>

#include "genesis/world/WorldBootstrap.hpp"
#include "genesis/world/WorldRegistry.hpp"

using genesis::world::LocationId;
using genesis::world::LocationKind;
using genesis::world::LocationNode;
using genesis::world::PathEdge;
using genesis::world::ResourceSpawn;
using genesis::world::ResourceType;
using genesis::world::WorldRegistry;

TEST(WorldRegistry, LoadsDemoGraph) {
    WorldRegistry world;
    world.setGraph(genesis::world::createDemoWorldGraph());

    EXPECT_EQ(world.locationCount(), 6);
    EXPECT_EQ(world.resourceSpawnCount(), 2);

    const auto tavernNodes = world.childrenOf(LocationId{2});
    EXPECT_EQ(tavernNodes.size(), 2);

    const auto* kitchen = world.findLocation(LocationId{3});
    ASSERT_NE(kitchen, nullptr);
    EXPECT_EQ(kitchen->kind, LocationKind::Room);
    EXPECT_FALSE(kitchen->navigable);

    const auto& edges = world.edgesFrom(LocationId{1});
    EXPECT_EQ(edges.size(), 2);
}

TEST(WorldRegistry, RejectsInvalidEntries) {
    WorldRegistry world;

    LocationNode root{LocationId{1}, genesis::world::InvalidLocation, "Root", LocationKind::Region, true};
    LocationNode child{LocationId{2}, LocationId{1}, "Child", LocationKind::Point, true};
    world.addLocation(root);
    world.addLocation(child);

    PathEdge invalidEdge{LocationId{2}, LocationId{3}, 1.0f, true};
    EXPECT_FALSE(world.addEdge(invalidEdge));

    ResourceSpawn invalidSpawn{"Broken", ResourceType::Food, LocationId{99}, 1, 1};
    world.addResourceSpawn(invalidSpawn);
    EXPECT_EQ(world.resourceSpawnCount(), 0);
}
