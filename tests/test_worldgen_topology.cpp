#include <gtest/gtest.h>

#include <set>
#include <vector>

#include "genesis/worldgen/TopologyModule.hpp"

using namespace genesis::worldgen;

namespace
{
TopologySettings make_default_settings()
{
    TopologySettings settings{};
    settings.cluster.min_clusters = 2;
    settings.cluster.max_clusters = 2;
    settings.cluster.min_nodes_per_cluster = 2;
    settings.cluster.max_nodes_per_cluster = 2;
    settings.corridor.enabled = true;
    settings.corridor.min_length = 3;
    settings.corridor.max_length = 3;
    return settings;
}

std::vector<std::pair<std::size_t, std::size_t>> edge_pairs(const TopologyDraft& draft)
{
    std::vector<std::pair<std::size_t, std::size_t>> result;
    result.reserve(draft.edges.size());
    for (const auto& edge : draft.edges)
    {
        result.emplace_back(edge.from, edge.to);
    }
    return result;
}
} // namespace

TEST(WorldgenTopology, DeterministicForSameSeed)
{
    TopologySettings settings = make_default_settings();
    TopologyModule module(settings);

    DeterministicRng rng_a(Seed{1337});
    DeterministicRng rng_b(Seed{1337});

    auto draft_a = module.generate(rng_a);
    auto draft_b = module.generate(rng_b);

    ASSERT_EQ(draft_a.nodes.size(), draft_b.nodes.size());
    ASSERT_EQ(draft_a.edges.size(), draft_b.edges.size());

    for (std::size_t i = 0; i < draft_a.nodes.size(); ++i)
    {
        EXPECT_EQ(draft_a.nodes[i].label, draft_b.nodes[i].label);
        EXPECT_EQ(draft_a.nodes[i].parent, draft_b.nodes[i].parent);
        EXPECT_EQ(draft_a.nodes[i].tags, draft_b.nodes[i].tags);
    }

    EXPECT_EQ(edge_pairs(draft_a), edge_pairs(draft_b));
}

TEST(WorldgenTopology, ClusterCountWithinExpectedRange)
{
    TopologySettings settings{};
    settings.cluster.min_clusters = 1;
    settings.cluster.max_clusters = 3;
    settings.cluster.min_nodes_per_cluster = 2;
    settings.cluster.max_nodes_per_cluster = 4;
    settings.corridor.enabled = false;

    TopologyModule module(settings);

    DeterministicRng rng(Seed{2024});
    auto draft = module.generate(rng);

    ASSERT_FALSE(draft.nodes.empty());
    const std::size_t total_nodes = draft.nodes.size();
    // 第一个节点是 root
    EXPECT_GE(total_nodes, 1 + settings.cluster.min_clusters * settings.cluster.min_nodes_per_cluster);
    EXPECT_LE(total_nodes, 1 + settings.cluster.max_clusters * settings.cluster.max_nodes_per_cluster);

    // 根节点标签
    EXPECT_EQ(draft.nodes.front().label, "root");
    EXPECT_FALSE(draft.edges.empty());
}

TEST(WorldgenTopology, CorridorCreatesLinearChain)
{
    TopologySettings settings{};
    settings.cluster.min_clusters = 1;
    settings.cluster.max_clusters = 1;
    settings.cluster.min_nodes_per_cluster = 1;
    settings.cluster.max_nodes_per_cluster = 1;
    settings.corridor.enabled = true;
    settings.corridor.min_length = 2;
    settings.corridor.max_length = 2;

    TopologyModule module(settings);

    DeterministicRng rng(Seed{77});
    auto draft = module.generate(rng);

    ASSERT_EQ(draft.edges.size(), draft.nodes.size() - 1);

    // 最后两个节点应为 corridor label
    ASSERT_GE(draft.nodes.size(), 3u);
    EXPECT_EQ(draft.nodes[draft.nodes.size() - 2].label, "corridor_0");
    EXPECT_EQ(draft.nodes[draft.nodes.size() - 1].label, "corridor_1");
}

