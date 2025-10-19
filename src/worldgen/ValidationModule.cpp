#include "genesis/worldgen/ValidationModule.hpp"

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

    return out_errors.empty();
}

} // namespace genesis::worldgen

