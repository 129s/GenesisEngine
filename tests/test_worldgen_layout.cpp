#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "genesis/worldgen/LayoutModule.hpp"
#include "genesis/worldgen/TopologyModule.hpp"

using namespace genesis::worldgen;

namespace
{
TopologySettings make_topology_settings()
{
    TopologySettings settings{};
    settings.cluster.min_clusters = 2;
    settings.cluster.max_clusters = 2;
    settings.cluster.min_nodes_per_cluster = 3;
    settings.cluster.max_nodes_per_cluster = 3;
    settings.corridor.enabled = true;
    settings.corridor.min_length = 2;
    settings.corridor.max_length = 2;
    return settings;
}

LayoutSettings make_layout_settings()
{
    LayoutSettings settings{};
    settings.grid.cell_width = 5.0;
    settings.grid.cell_height = 4.0;
    settings.grid.columns = 2;
    settings.grid.margin = 1.0;
    settings.cluster.radial_distance = 16.0;
    settings.cluster.radial_step = 6.0;
    settings.cluster.node_spacing = 1.5;
    settings.corridor.step = 7.0;
    settings.hex.enabled = false;
    return settings;
}

const NodeDraft* find_node(const TopologyDraft& draft, std::size_t local_id)
{
    auto it = std::find_if(draft.nodes.begin(), draft.nodes.end(), [&](const NodeDraft& node) {
        return node.local_id == local_id;
    });
    return it != draft.nodes.end() ? &(*it) : nullptr;
}

TEST(WorldgenLayout, HexEnabledCreatesHoneycombPattern)
{
    TopologySettings topo = make_topology_settings();
    topo.cluster.min_clusters = 1;
    topo.cluster.max_clusters = 1;
    topo.cluster.min_nodes_per_cluster = 7;
    topo.cluster.max_nodes_per_cluster = 7;
    topo.corridor.enabled = false;

    LayoutSettings layout_settings = make_layout_settings();
    layout_settings.hex.enabled = true;
    layout_settings.hex.spacing = 4.5;

    TopologyModule topology(topo);
    DeterministicRng topo_rng(Seed{888});
    auto draft = topology.generate(topo_rng);

    LayoutModule layout_module(layout_settings);
    DeterministicRng layout_rng(Seed{123});
    auto placements = layout_module.generate(draft, layout_rng);

    const double anchor_x = layout_settings.cluster.radial_distance;
    const double anchor_y = 0.0;

    std::vector<std::pair<double, double>> offsets;
    for (const auto& placement : placements.placements)
    {
        const auto* node = find_node(draft, placement.local_id);
        if (node && node->label == "cluster_0")
        {
            offsets.emplace_back(placement.x - anchor_x, placement.y - anchor_y);
        }
    }

    ASSERT_EQ(offsets.size(), 7u);

    const double spacing = layout_settings.hex.spacing;
    const double sqrt3 = std::sqrt(3.0);
    const std::vector<std::pair<double, double>> expected = {
        {0.0, 0.0},
        {sqrt3 * spacing, 0.0},
        {sqrt3 / 2.0 * spacing, 1.5 * spacing},
        {-sqrt3 / 2.0 * spacing, 1.5 * spacing},
        {-sqrt3 * spacing, 0.0},
        {-sqrt3 / 2.0 * spacing, -1.5 * spacing},
        {sqrt3 / 2.0 * spacing, -1.5 * spacing},
    };

    auto matches = [&](const std::pair<double, double>& target) {
        for (const auto& offset : offsets)
        {
            if (std::abs(offset.first - target.first) < 1e-3 &&
                std::abs(offset.second - target.second) < 1e-3)
            {
                return true;
            }
        }
        return false;
    };

    for (const auto& expected_offset : expected)
    {
        EXPECT_TRUE(matches(expected_offset));
    }
}

const NodePlacement* find_placement(const LayoutDraft& layout, std::size_t local_id)
{
    auto it = std::find_if(layout.placements.begin(), layout.placements.end(), [&](const NodePlacement& placement) {
        return placement.local_id == local_id;
    });
    return it != layout.placements.end() ? &(*it) : nullptr;
}

bool is_cluster_node(const NodeDraft& node)
{
    return std::find(node.tags.begin(), node.tags.end(), "cluster") != node.tags.end();
}

bool is_corridor_node(const NodeDraft& node)
{
    return std::find(node.tags.begin(), node.tags.end(), "corridor") != node.tags.end();
}
} // namespace

TEST(WorldgenLayout, DeterministicPlacements)
{
    TopologyModule topology(make_topology_settings());
    DeterministicRng topo_rng_a(Seed{99});
    auto draft = topology.generate(topo_rng_a);

    LayoutModule layout(make_layout_settings());
    DeterministicRng rng_a(Seed{777});
    DeterministicRng rng_b(Seed{777});

    auto placements_a = layout.generate(draft, rng_a);
    auto placements_b = layout.generate(draft, rng_b);

    ASSERT_EQ(placements_a.placements.size(), placements_b.placements.size());
    for (std::size_t i = 0; i < placements_a.placements.size(); ++i)
    {
        EXPECT_EQ(placements_a.placements[i].local_id, placements_b.placements[i].local_id);
        EXPECT_DOUBLE_EQ(placements_a.placements[i].x, placements_b.placements[i].x);
        EXPECT_DOUBLE_EQ(placements_a.placements[i].y, placements_b.placements[i].y);
    }
}

TEST(WorldgenLayout, RootPlacedAtOrigin)
{
    TopologyModule topology(make_topology_settings());
    DeterministicRng topo_rng(Seed{42});
    auto draft = topology.generate(topo_rng);

    LayoutModule layout(make_layout_settings());
    DeterministicRng layout_rng_origin(Seed{1});
    auto placements = layout.generate(draft, layout_rng_origin);

    const auto* root_node = find_node(draft, 0);
    ASSERT_NE(root_node, nullptr);
    const auto* root_placement = find_placement(placements, root_node->local_id);
    ASSERT_NE(root_placement, nullptr);
    EXPECT_DOUBLE_EQ(root_placement->x, 0.0);
    EXPECT_DOUBLE_EQ(root_placement->y, 0.0);
}

TEST(WorldgenLayout, ClusterNodesSpreadAroundCircle)
{
    TopologyModule topology(make_topology_settings());
    DeterministicRng topo_rng(Seed{2024});
    auto draft = topology.generate(topo_rng);

    LayoutModule layout(make_layout_settings());
    DeterministicRng layout_rng_circle(Seed{2024});
    auto placements = layout.generate(draft, layout_rng_circle);

    std::unordered_map<std::string, std::vector<const NodePlacement*>> clusters;
    for (const auto& node : draft.nodes)
    {
        if (!is_cluster_node(node))
        {
            continue;
        }
        const auto* placement = find_placement(placements, node.local_id);
        ASSERT_NE(placement, nullptr);
        clusters[node.label].push_back(placement);
    }

    ASSERT_FALSE(clusters.empty());

    for (const auto& [label, group] : clusters)
    {
        ASSERT_FALSE(group.empty()) << label;

        double average_radius = 0.0;
        for (const auto* placement : group)
        {
            const double radius = std::hypot(placement->x, placement->y);
            average_radius += radius;
        }
        average_radius /= static_cast<double>(group.size());

        EXPECT_GT(average_radius, 10.0);
    }
}

TEST(WorldgenLayout, CorridorNodesPlacedLinearlyAlongX)
{
    TopologyModule topology(make_topology_settings());
    DeterministicRng topo_rng(Seed{9});
    auto draft = topology.generate(topo_rng);

    LayoutModule layout(make_layout_settings());
    DeterministicRng layout_rng_corridor(Seed{9});
    auto placements = layout.generate(draft, layout_rng_corridor);

    std::vector<const NodePlacement*> corridor;
    for (const auto& node : draft.nodes)
    {
        if (!is_corridor_node(node))
        {
            continue;
        }
        const auto* placement = find_placement(placements, node.local_id);
        ASSERT_NE(placement, nullptr);
        corridor.push_back(placement);
    }

    if (corridor.empty())
    {
        GTEST_SKIP() << "当前拓扑未生成走廊节点";
    }

    std::sort(corridor.begin(), corridor.end(), [](const NodePlacement* a, const NodePlacement* b) {
        return a->x < b->x;
    });

    for (std::size_t i = 1; i < corridor.size(); ++i)
    {
        EXPECT_GT(corridor[i]->x, corridor[i - 1]->x);
        EXPECT_NEAR(corridor[i]->y, 0.0, 1e-5);
    }
}

TEST(WorldgenLayout, NoiseLayoutRespectsSpacing)
{
    LayoutSettings layout_settings = make_layout_settings();
    layout_settings.hex.enabled = false;
    layout_settings.noise.enabled = true;
    layout_settings.noise.radius = 12.0;
    layout_settings.noise.min_spacing = 3.0;
    layout_settings.noise.max_attempts = 128;

    TopologyDraft topology{};
    NodeDraft root{};
    root.local_id = 0;
    root.label = "root";
    root.tags = {"hub"};
    topology.nodes.push_back(root);

    for (std::size_t i = 0; i < 5; ++i)
    {
        NodeDraft node{};
        node.local_id = static_cast<std::size_t>(i + 1);
        node.parent = 0;
        node.label = "wild";
        node.tags = {"wilds"};
        topology.nodes.push_back(node);
    }

    LayoutModule module(layout_settings);
    DeterministicRng rng_a(Seed{4321});
    auto layout_a = module.generate(topology, rng_a);
    DeterministicRng rng_b(Seed{4321});
    auto layout_b = module.generate(topology, rng_b);

    ASSERT_EQ(layout_a.placements.size(), layout_b.placements.size());
    for (std::size_t i = 0; i < layout_a.placements.size(); ++i)
    {
        EXPECT_EQ(layout_a.placements[i].local_id, layout_b.placements[i].local_id);
        EXPECT_DOUBLE_EQ(layout_a.placements[i].x, layout_b.placements[i].x);
        EXPECT_DOUBLE_EQ(layout_a.placements[i].y, layout_b.placements[i].y);
    }

    std::vector<std::pair<double, double>> noise_positions;
    for (const auto& placement : layout_a.placements)
    {
        if (placement.local_id == 0)
        {
            continue;
        }
        noise_positions.emplace_back(placement.x, placement.y);
    }

    const double min_spacing = layout_settings.noise.min_spacing - 1e-6;
    for (std::size_t i = 0; i < noise_positions.size(); ++i)
    {
        for (std::size_t j = i + 1; j < noise_positions.size(); ++j)
        {
            const double dx = noise_positions[i].first - noise_positions[j].first;
            const double dy = noise_positions[i].second - noise_positions[j].second;
            const double dist = std::sqrt(dx * dx + dy * dy);
            EXPECT_GT(dist, min_spacing);
        }
    }
}
