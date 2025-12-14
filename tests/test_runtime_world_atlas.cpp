#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "genesis/runtime/Runtime.hpp"

namespace {

void writeFile(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream ofs(path, std::ios::binary);
    ofs << text;
}

std::filesystem::path makeTempWorldDir() {
    const auto stamp = std::to_string(static_cast<long long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    const auto tmp = std::filesystem::temp_directory_path() / ("genesis_runtime_world_atlas_" + stamp);
    std::filesystem::create_directories(tmp);
    return tmp;
}

} // namespace

TEST(RuntimeWorldAtlas, ExposesAtlasAndWorldVersionAfterLoad) {
    const auto tmp = makeTempWorldDir();

    writeFile(tmp / "world.json", R"JSON({
  "maps": [
    { "id": 1, "name": "A" },
    { "id": 2, "name": "B" }
  ],
  "map_edges": [
    { "from": 1, "to": 2, "cost": 3.5, "bidirectional": true, "rules": { "extraCost": 1.0 } }
  ]
})JSON");

    writeFile(tmp / "map_1.json", R"JSON({
  "scenes": [ { "id": 100, "name": "S", "origin": [10, 20] } ],
  "interactions": [
    { "id": 1000, "sceneId": 100, "kind": "Resource", "coord_local": [2,3], "name": "Fountain" }
  ],
  "portals": [ { "interactionId": 1000, "targetMapId": 2, "channelId": "door", "oneWay": false, "teleportCost": 0.5 } ],
  "tilemap": { "width": 64, "height": 48, "tileW": 16, "tileH": 16 }
})JSON");

    writeFile(tmp / "map_2.json", R"JSON({
  "scenes": [ { "id": 200, "name": "S2" } ],
  "interactions": [],
  "portals": []
})JSON");

    Genesis::Runtime::RuntimeConfig config{};
    config.initialWorldPath = tmp;
    Genesis::Runtime::Runtime runtime(config);

    EXPECT_EQ(runtime.worldVersion(), 1U);

    auto atlas = runtime.worldAtlas();
    ASSERT_TRUE(atlas);
    EXPECT_EQ(atlas->world_version, runtime.worldVersion());
    ASSERT_EQ(atlas->maps.size(), 2U);
    ASSERT_EQ(atlas->mapEdges.size(), 1U);
    EXPECT_NEAR(atlas->mapEdges.front().cost, 3.5, 1e-6);
    ASSERT_TRUE(atlas->mapEdges.front().rules.has_value());

    ASSERT_EQ(atlas->perMap.size(), 2U);
    const auto& per0 = atlas->perMap.front();
    ASSERT_EQ(per0.mapId, 1U);
    ASSERT_TRUE(per0.tilemap.has_value());
    EXPECT_EQ(per0.tilemap->width, 64);

    ASSERT_EQ(per0.interactions.size(), 1U);
    ASSERT_TRUE(per0.interactions.front().coordGlobal.has_value());
    EXPECT_EQ(per0.interactions.front().coordGlobal->first, 12);
    EXPECT_EQ(per0.interactions.front().coordGlobal->second, 23);

    ASSERT_EQ(per0.portals.size(), 1U);
    EXPECT_EQ(per0.portals.front().mapId, 1U);
    ASSERT_TRUE(per0.portals.front().channelId.has_value());
    EXPECT_EQ(*per0.portals.front().channelId, "door");

    const auto loadAgain = runtime.loadWorldFromFile(tmp);
    ASSERT_TRUE(loadAgain.success) << loadAgain.error;
    EXPECT_EQ(runtime.worldVersion(), 2U);
}

