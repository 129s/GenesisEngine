#include "genesis/worldgen/ValidationModule.hpp"

#include <cmath>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace genesis::worldgen
{

namespace
{
struct LayoutIndex
{
    std::unordered_map<std::size_t, const NodePlacement*> placements;
};

LayoutIndex build_layout_index(const LayoutDraft& layout)
{
    LayoutIndex index{};
    index.placements.reserve(layout.placements.size());
    for (const auto& placement : layout.placements)
    {
        index.placements.emplace(placement.local_id, &placement);
    }
    return index;
}

void append_error(std::vector<ValidationError>& errors, std::string message)
{
    errors.push_back(ValidationError{std::move(message)});
}

std::string edge_to_string(const EdgeDraft& edge)
{
    std::ostringstream oss;
    oss << "(" << edge.from << " -> " << edge.to << ")";
    return oss.str();
}
} // namespace

bool ValidationModule::validate(const TopologyDraft& topology,
    const LayoutDraft& layout,
    std::vector<ValidationError>& out_errors) const
{
    out_errors.clear();

    if (topology.nodes.empty())
    {
        append_error(out_errors, "拓扑节点列表为空");
        return false;
    }

    std::unordered_set<std::size_t> seen_nodes;
    seen_nodes.reserve(topology.nodes.size());

    constexpr double kMaxCoordinateMagnitude = 100000.0;

    for (const auto& node : topology.nodes)
    {
        if (!seen_nodes.insert(node.local_id).second)
        {
            std::ostringstream oss;
            oss << "节点 local_id 重复: " << node.local_id;
            append_error(out_errors, oss.str());
        }
    }

    LayoutIndex layout_index = build_layout_index(layout);

    for (const auto& placement : layout.placements)
    {
        if (!std::isfinite(placement.x) || !std::isfinite(placement.y))
        {
            std::ostringstream oss;
            oss << "节点 " << placement.local_id << " 的坐标非有限值";
            append_error(out_errors, oss.str());
            continue;
        }
        if (std::abs(placement.x) > kMaxCoordinateMagnitude || std::abs(placement.y) > kMaxCoordinateMagnitude)
        {
            std::ostringstream oss;
            oss << "节点 " << placement.local_id << " 坐标超出边界: (" << placement.x << ", " << placement.y << ")";
            append_error(out_errors, oss.str());
        }
    }

    for (const auto& node : topology.nodes)
    {
        if (layout_index.placements.find(node.local_id) == layout_index.placements.end())
        {
            std::ostringstream oss;
            oss << "节点缺少布局坐标: " << node.local_id;
            append_error(out_errors, oss.str());
        }
    }

    for (const auto& edge : topology.edges)
    {
        if (!seen_nodes.contains(edge.from))
        {
            std::ostringstream oss;
            oss << "边的起点不存在: " << edge_to_string(edge);
            append_error(out_errors, oss.str());
        }
        if (!seen_nodes.contains(edge.to))
        {
            std::ostringstream oss;
            oss << "边的终点不存在: " << edge_to_string(edge);
            append_error(out_errors, oss.str());
        }
        if (edge.from == edge.to)
        {
            std::ostringstream oss;
            oss << "边指向自身: " << edge_to_string(edge);
            append_error(out_errors, oss.str());
        }
    }

    struct EdgeKeyHash
    {
        std::size_t operator()(const std::pair<std::size_t, std::size_t>& key) const noexcept
        {
            return std::hash<std::size_t>{}(key.first) ^ (std::hash<std::size_t>{}(key.second) << 1);
        }
    };
    std::unordered_set<std::pair<std::size_t, std::size_t>, EdgeKeyHash> edge_set;
    edge_set.reserve(topology.edges.size());
    for (const auto& edge : topology.edges)
    {
        edge_set.emplace(edge.from, edge.to);
        if (edge.bidirectional)
        {
            edge_set.emplace(edge.to, edge.from);
        }
    }

    for (const auto& portal : topology.portals)
    {
        if (!seen_nodes.contains(portal.entry) || !seen_nodes.contains(portal.exit))
        {
            std::ostringstream oss;
            oss << "Portal 节点不存在: entry=" << portal.entry << ", exit=" << portal.exit;
            append_error(out_errors, oss.str());
            continue;
        }

        if (!edge_set.contains({portal.entry, portal.exit}) || !edge_set.contains({portal.exit, portal.entry}))
        {
            std::ostringstream oss;
            oss << "Portal 缺少双向边: entry=" << portal.entry << ", exit=" << portal.exit;
            append_error(out_errors, oss.str());
        }
    }

    return out_errors.empty();
}

} // namespace genesis::worldgen
