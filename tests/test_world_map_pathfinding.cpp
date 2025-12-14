#include <gtest/gtest.h>

#include "genesis/world/MapPathfinding.hpp"

using namespace genesis::world;

TEST(WorldMapPathfinding, ChoosesCheapestCostPath) {
    InMemoryWorldDatabase db;
    db.clear();

    db.addMap(Map{1, "A"});
    db.addMap(Map{2, "B"});
    db.addMap(Map{3, "C"});

    MapEdge direct{};
    direct.from = 1;
    direct.to = 2;
    direct.cost = 10.0;
    db.addMapEdge(direct);

    MapEdge hop1{};
    hop1.from = 1;
    hop1.to = 3;
    hop1.cost = 1.0;
    db.addMapEdge(hop1);

    MapEdge hop2{};
    hop2.from = 3;
    hop2.to = 2;
    hop2.cost = 1.0;
    db.addMapEdge(hop2);

    const auto path = shortestMapPath(db, 1, 2);
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path->maps.size(), 3U);
    EXPECT_EQ(path->maps[0], 1U);
    EXPECT_EQ(path->maps[1], 3U);
    EXPECT_EQ(path->maps[2], 2U);
    EXPECT_NEAR(path->totalCost, 2.0, 1e-6);
}

TEST(WorldMapPathfinding, RespectsDisabledRule) {
    InMemoryWorldDatabase db;
    db.clear();

    db.addMap(Map{1, "A"});
    db.addMap(Map{2, "B"});
    db.addMap(Map{3, "C"});

    MapEdge direct{};
    direct.from = 1;
    direct.to = 2;
    direct.cost = 10.0;
    db.addMapEdge(direct);

    MapEdge blocked{};
    blocked.from = 1;
    blocked.to = 3;
    blocked.cost = 1.0;
    blocked.rules = nlohmann::json{{"disabled", true}};
    db.addMapEdge(blocked);

    MapEdge hop2{};
    hop2.from = 3;
    hop2.to = 2;
    hop2.cost = 1.0;
    db.addMapEdge(hop2);

    const auto path = shortestMapPath(db, 1, 2);
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path->maps.size(), 2U);
    EXPECT_EQ(path->maps[0], 1U);
    EXPECT_EQ(path->maps[1], 2U);
    EXPECT_NEAR(path->totalCost, 10.0, 1e-6);
}

