#include "genesis/worldgen/TopologyModule.hpp"

#include <stdexcept>
#include <string>

namespace genesis::worldgen
{

namespace
{
std::string make_cluster_label(std::size_t cluster_index)
{
    return "cluster_" + std::to_string(cluster_index);
}

std::string make_corridor_label(std::size_t segment_index)
{
    return "corridor_" + std::to_string(segment_index);
}
} // namespace

TopologyModule::TopologyModule(TopologySettings settings)
    : settings_(std::move(settings))
{
    if (settings_.cluster.min_clusters == 0 || settings_.cluster.max_clusters == 0)
    {
        throw std::invalid_argument("cluster 配置必须至少生成 1 个簇");
    }
    if (settings_.cluster.min_clusters > settings_.cluster.max_clusters)
    {
        throw std::invalid_argument("cluster min_clusters 不能大于 max_clusters");
    }
    if (settings_.cluster.min_nodes_per_cluster == 0 || settings_.cluster.max_nodes_per_cluster == 0)
    {
        throw std::invalid_argument("cluster 节点数必须为正");
    }
    if (settings_.cluster.min_nodes_per_cluster > settings_.cluster.max_nodes_per_cluster)
    {
        throw std::invalid_argument("cluster min_nodes_per_cluster 不能大于 max_nodes_per_cluster");
    }
    if (settings_.corridor.enabled && settings_.corridor.min_length > settings_.corridor.max_length)
    {
        throw std::invalid_argument("corridor min_length 不能大于 max_length");
    }
}

std::size_t TopologyModule::random_inclusive(DeterministicRng& rng, std::size_t min, std::size_t max) const
{
    if (min == max)
    {
        return min;
    }
    const auto span = max - min + 1;
    return min + rng.uniform_index(span);
}

TopologyDraft TopologyModule::generate(DeterministicRng& rng) const
{
    TopologyDraft draft{};
    draft.nodes.reserve(8);
    draft.edges.reserve(8);

    // 根节点
    NodeDraft root{};
    root.local_id = 0;
    root.kind = DraftNodeKind::Scene;
    root.label = "root";
    root.parent.reset();
    root.tags = {"hub"};
    draft.nodes.push_back(root);

    auto allocation_rng = rng.fork(0xC11157EFull);

    const auto cluster_count =
        random_inclusive(allocation_rng, settings_.cluster.min_clusters, settings_.cluster.max_clusters);

    std::size_t next_id = 1;
    for (std::size_t cluster_index = 0; cluster_index < cluster_count; ++cluster_index)
    {
        const auto nodes_in_cluster = random_inclusive(allocation_rng,
            settings_.cluster.min_nodes_per_cluster,
            settings_.cluster.max_nodes_per_cluster);

        std::size_t previous_id = 0; // connect to root first
        for (std::size_t node_index = 0; node_index < nodes_in_cluster; ++node_index)
        {
            NodeDraft node{};
            node.local_id = next_id;
            node.kind = DraftNodeKind::Scene;
            node.label = make_cluster_label(cluster_index);
            node.parent = previous_id;
            node.tags = {"cluster"};
            draft.nodes.push_back(node);

            draft.edges.push_back(EdgeDraft{previous_id, next_id, true});

            previous_id = next_id;
            ++next_id;
        }
    }

    if (settings_.corridor.enabled)
    {
        const auto corridor_length =
            random_inclusive(allocation_rng, settings_.corridor.min_length, settings_.corridor.max_length);

        std::size_t attach_from = 0;
        if (!draft.nodes.empty())
        {
            attach_from = draft.nodes.back().local_id;
        }

        for (std::size_t segment = 0; segment < corridor_length; ++segment)
        {
            NodeDraft node{};
            node.local_id = next_id;
            node.kind = DraftNodeKind::Scene;
            node.label = make_corridor_label(segment);
            node.parent = attach_from;
            node.tags = {"corridor"};
            draft.nodes.push_back(node);

            draft.edges.push_back(EdgeDraft{attach_from, next_id, true});
            attach_from = next_id;
            ++next_id;
        }
    }

    return draft;
}

} // namespace genesis::worldgen
