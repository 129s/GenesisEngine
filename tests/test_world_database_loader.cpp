#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "genesis/world/WorldDatabaseLoader.hpp"

using namespace genesis::world;

namespace {
void writeFile(const std::filesystem::path& p, const std::string& text) {
    std::filesystem::create_directories(p.parent_path());
    std::ofstream ofs(p, std::ios::binary);
    ofs << text;
}
}

TEST(WorldDatabaseLoader, LoadsMinimalDataset) {
    const auto tmp = std::filesystem::temp_directory_path() / ("genesis_world_db_test_" + std::to_string(static_cast<long long>(std::chrono::high_resolution_clock::now().time_since_epoch().count())));
    std::filesystem::create_directories(tmp);

    writeFile(tmp / "world.json", R"JSON({
  "maps": [
    { "id": 1, "name": "A" },
    { "id": 2, "name": "B" }
  ],
  "map_edges": [
    { "from": 1, "to": 2, "bidirectional": true }
  ]
})JSON");

    writeFile(tmp / "map_1.json", R"JSON({
  "scenes": [ { "id": 100, "name": "S" } ],
  "interactions": [ { "id": 1000, "sceneId": 100, "kind": "Resource", "resourceType": "Drink", "coord": [2,3], "name": "Fountain" } ],
  "portals": [ { "interactionId": 1000, "targetMapId": 2 } ],
  "tilemap": { "width": 64, "height": 48, "tileW": 16, "tileH": 16 }
})JSON");

    const auto result = loadWorldDatabaseFromFolder(tmp);
    ASSERT_TRUE(result.success) << result.error;
    ASSERT_TRUE(result.database);

    const auto& db = *result.database;
    ASSERT_EQ(db.maps().size(), 2U);
    ASSERT_EQ(db.mapEdges().size(), 1U);
    EXPECT_EQ(db.scenes(1).size(), 1U);
    EXPECT_EQ(db.interactions(1).size(), 1U);
    ASSERT_TRUE(db.interactions(1).front().resourceType.has_value());
    EXPECT_EQ(*db.interactions(1).front().resourceType, ResourceType::Drink);
    ASSERT_EQ(db.portals(1).size(), 1U);
    EXPECT_EQ(db.portals(1).front().mapId, 1U);
    EXPECT_EQ(db.portals(1).front().targetMapId, 2U);

    const auto tile = db.tilemap(1);
    ASSERT_TRUE(tile.has_value());
    EXPECT_EQ(tile->width, 64);
    EXPECT_EQ(tile->height, 48);
    EXPECT_EQ(tile->tileW, 16);
    EXPECT_EQ(tile->tileH, 16);

    const auto edgesFrom2 = db.mapEdgesFrom(2);
    ASSERT_EQ(edgesFrom2.size(), 1U);
    EXPECT_EQ(edgesFrom2.front().from, 2U);
    EXPECT_EQ(edgesFrom2.front().to, 1U);
}
