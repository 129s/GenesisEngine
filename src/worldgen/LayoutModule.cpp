#include "genesis/worldgen/LayoutModule.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace genesis::worldgen
{

namespace
{
bool has_tag(const NodeDraft& node, std::string_view tag)
{
    return std::find(node.tags.begin(), node.tags.end(), tag) != node.tags.end();
}

template<typename TValue>
void ensure_positive(TValue value, std::string_view field)
{
    if (value <= static_cast<TValue>(0))
    {
        throw std::invalid_argument(std::string(field) + " 必须为正数");
    }
}

struct ClusterGroup
{
    std::string label;
    std::vector<const NodeDraft*> nodes;
};
} // namespace

LayoutModule::LayoutModule(LayoutSettings settings)
    : settings_(std::move(settings))
{
    ensure_positive(settings_.grid.cell_width, "grid.cell_width");
    ensure_positive(settings_.grid.cell_height, "grid.cell_height");
    if (settings_.grid.columns == 0)
    {
        throw std::invalid_argument("grid.columns 必须大于 0");
    }
    ensure_positive(settings_.cluster.radial_distance, "cluster.radial_distance");
    ensure_positive(settings_.cluster.radial_step, "cluster.radial_step");
    ensure_positive(settings_.corridor.step, "corridor.step");
}

LayoutDraft LayoutModule::generate(const TopologyDraft& topology, DeterministicRng&) const
{
    LayoutDraft layout{};
    layout.placements.reserve(topology.nodes.size());

    std::unordered_set<std::size_t> placed;
    placed.reserve(topology.nodes.size());

    // 1. Root/hub
    for (const auto& node : topology.nodes)
    {
        if (has_tag(node, "hub") || node.label == "root")
        {
            layout.placements.push_back(NodePlacement{node.local_id, 0.0, 0.0, 0.0});
            placed.insert(node.local_id);
            break;
        }
    }

    // 2. Cluster grouping
    std::vector<ClusterGroup> cluster_groups;
    cluster_groups.reserve(topology.nodes.size());
    for (const auto& node : topology.nodes)
    {
        if (placed.contains(node.local_id))
        {
            continue;
        }

        if (has_tag(node, "cluster"))
        {
            auto it = std::find_if(cluster_groups.begin(), cluster_groups.end(), [&](const ClusterGroup& group) {
                return group.label == node.label;
            });
            if (it == cluster_groups.end())
            {
                cluster_groups.push_back(ClusterGroup{node.label, {&node}});
            }
            else
            {
                it->nodes.push_back(&node);
            }
        }
    }

    const std::size_t cluster_count = cluster_groups.size();
    const auto columns = static_cast<double>(settings_.grid.columns);

    constexpr double TWO_PI = 6.28318530717958647692;

    for (std::size_t index = 0; index < cluster_groups.size(); ++index)
    {
        auto& group = cluster_groups[index];
        std::sort(group.nodes.begin(), group.nodes.end(), [](const NodeDraft* a, const NodeDraft* b) {
            return a->local_id < b->local_id;
        });

        const double angle = cluster_count > 0 ? (TWO_PI * static_cast<double>(index) / static_cast<double>(cluster_count))
                                               : 0.0;
        const double radius = settings_.cluster.radial_distance + settings_.cluster.radial_step * static_cast<double>(index);

        const double anchor_x = std::cos(angle) * radius;
        const double anchor_y = std::sin(angle) * radius;

        for (std::size_t node_index = 0; node_index < group.nodes.size(); ++node_index)
        {
            const auto* node = group.nodes[node_index];
            const double col = static_cast<double>(node_index % settings_.grid.columns);
            const double row = static_cast<double>(node_index / settings_.grid.columns);

            const double offset_x = (col - (columns - 1.0) / 2.0) * settings_.grid.cell_width;
            const double offset_y = row * (settings_.grid.cell_height + settings_.cluster.node_spacing);

            layout.placements.push_back(NodePlacement{
                node->local_id,
                anchor_x + offset_x,
                anchor_y + offset_y,
                0.0});
            placed.insert(node->local_id);
        }
    }

    // 3. Corridor nodes (linear chain)
    std::vector<const NodeDraft*> corridor_nodes;
    for (const auto& node : topology.nodes)
    {
        if (!placed.contains(node.local_id) && has_tag(node, "corridor"))
        {
            corridor_nodes.push_back(&node);
        }
    }
    std::sort(corridor_nodes.begin(), corridor_nodes.end(), [](const NodeDraft* a, const NodeDraft* b) {
        return a->local_id < b->local_id;
    });

    double corridor_offset = settings_.cluster.radial_distance +
        settings_.cluster.radial_step * static_cast<double>(cluster_count + 1);

    for (std::size_t i = 0; i < corridor_nodes.size(); ++i)
    {
        const auto* node = corridor_nodes[i];
        const double x = corridor_offset + settings_.corridor.step * static_cast<double>(i + 1);
        layout.placements.push_back(NodePlacement{node->local_id, x, 0.0, 0.0});
        placed.insert(node->local_id);
    }

    // 4. Fallback for nodes without placement
    for (const auto& node : topology.nodes)
    {
        if (!placed.contains(node.local_id))
        {
            layout.placements.push_back(NodePlacement{node.local_id, 0.0, 0.0, 0.0});
            placed.insert(node.local_id);
        }
    }

    std::sort(layout.placements.begin(),
        layout.placements.end(),
        [](const NodePlacement& a, const NodePlacement& b) { return a.local_id < b.local_id; });

    return layout;
}

} // namespace genesis::worldgen

