#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "genesis/world/WorldBootstrap.hpp"
#include "genesis/world/WorldLoader.hpp"
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

TEST(WorldRegistry, EnumeratesLocations) {
    WorldRegistry world;
    world.setGraph(genesis::world::createDemoWorldGraph());

    const auto nodes = world.locations();
    EXPECT_EQ(nodes.size(), world.locationCount());

    const bool containsRoot = std::any_of(nodes.begin(), nodes.end(), [](const LocationNode& node) {
        return node.id == LocationId{1};
    });
    EXPECT_TRUE(containsRoot);
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

TEST(WorldLoader, ParsesJsonString) {
    constexpr std::string_view jsonData = R"json(
        {
            "locations": [
                { "id": 1, "parent": 0, "name": "Root", "kind": "Region" },
                { "id": 2, "parent": 1, "name": "Home", "kind": "Building" }
            ],
            "edges": [
                { "from": 1, "to": 2 }
            ],
            "spawns": [
                { "name": "Kitchen", "type": "Food", "location": 2, "capacity": 5, "rate_per_step": 1 }
            ]
        }
    )json";

    WorldRegistry world;
    auto result = genesis::world::loadWorldFromJsonString(jsonData, world);
    ASSERT_TRUE(result.success) << result.error;
    EXPECT_EQ(world.locationCount(), 2);
    EXPECT_EQ(world.resourceSpawnCount(), 1);
}

TEST(WorldLoader, ReportsInvalidResourceType) {
    constexpr std::string_view jsonData = R"json(
        {
            "locations": [
                { "id": 1, "parent": 0, "name": "Root", "kind": "Region" }
            ],
            "spawns": [
                { "name": "Broken", "type": "Magic", "location": 1 }
            ]
        }
    )json";

    WorldRegistry world;
    auto result = genesis::world::loadWorldFromJsonString(jsonData, world);
    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error.find("Unknown resource type"), std::string::npos);
}

TEST(WorldLoader, LoadsFromFile) {
    const auto tempPath = std::filesystem::temp_directory_path() / "genesis_loader_test.json";

    std::ofstream out(tempPath);
    out << R"json({
        "locations": [
            { "id": 1, "parent": 0, "name": "Root", "kind": "Region" }
        ]
    })json";
    out.close();

    WorldRegistry world;
    auto result = genesis::world::loadWorldFromFile(tempPath, world);
    EXPECT_TRUE(result.success) << result.error;
    EXPECT_EQ(world.locationCount(), 1);

    std::filesystem::remove(tempPath);
}
