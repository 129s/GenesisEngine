#include <gtest/gtest.h>

#include "genesis/worldgen/Generator.hpp"

using namespace genesis::worldgen;

namespace
{
GeneratorConfig make_config()
{
    GeneratorConfig cfg{};
    cfg.topology.cluster.min_clusters = 2;
    cfg.topology.cluster.max_clusters = 2;
    cfg.topology.cluster.min_nodes_per_cluster = 2;
    cfg.topology.cluster.max_nodes_per_cluster = 3;
    cfg.topology.corridor.enabled = true;
    cfg.topology.corridor.min_length = 2;
    cfg.topology.corridor.max_length = 2;

    cfg.layout.grid.cell_width = 5.0;
    cfg.layout.grid.cell_height = 4.0;
    cfg.layout.grid.columns = 2;
    cfg.layout.grid.margin = 1.0;

    cfg.layout.cluster.radial_distance = 12.0;
    cfg.layout.cluster.radial_step = 4.0;
    cfg.layout.cluster.node_spacing = 2.0;

    cfg.layout.corridor.step = 6.0;

    return cfg;
}
} // namespace

TEST(WorldgenGenerator, ProducesWorldGraph)
{
    auto cfg = make_config();
    const Seed seed{12345};
    auto result = generate_world(cfg, seed);

    EXPECT_GT(result.location_count, 0u);
    EXPECT_EQ(result.location_count, result.world_graph.nodes.size());
    EXPECT_EQ(result.edge_count, result.world_graph.edges.size());
    EXPECT_EQ(result.seed.value, seed.value);

    ASSERT_FALSE(result.world_graph.nodes.empty());
    const auto& root = result.world_graph.nodes.front();
    EXPECT_EQ(root.parent, genesis::world::InvalidLocation);
    EXPECT_TRUE(root.coord_global.has_value());

    EXPECT_FALSE(result.logs.empty());
}

