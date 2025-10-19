#include "genesis/worldgen/Generator.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "genesis/world/WorldTypes.hpp"
#include "genesis/worldgen/LayoutModule.hpp"
#include "genesis/worldgen/TilemapModule.hpp"
#include "genesis/worldgen/TopologyModule.hpp"
#include "genesis/worldgen/ValidationModule.hpp"

namespace genesis::worldgen
{

namespace
{
bool has_tag(const NodeDraft& node, std::string_view tag)
{
    return std::find(node.tags.begin(), node.tags.end(), tag) != node.tags.end();
}

world::LocationKind infer_kind(const NodeDraft& node)
{
    if (node.local_id == 0 || has_tag(node, "hub"))
    {
        return world::LocationKind::Region;
    }
    if (has_tag(node, "cluster"))
    {
        return world::LocationKind::Building;
    }
    if (has_tag(node, "corridor"))
    {
        return world::LocationKind::Room;
    }
    return world::LocationKind::Point;
}

struct GraphBuildResult
{
    world::LocationGraph graph;
    std::unordered_map<std::size_t, world::LocationId> id_map;
};

std::unordered_map<std::size_t, NodePlacement> build_placement_map(const LayoutDraft& layout)
{
    std::unordered_map<std::size_t, NodePlacement> map;
    map.reserve(layout.placements.size());
    for (const auto& placement : layout.placements)
    {
        map.emplace(placement.local_id, placement);
    }
    return map;
}

GraphBuildResult build_location_graph(const TopologyDraft& topology, const LayoutDraft& layout)
{
    GraphBuildResult result{};
    auto& graph = result.graph;
    graph.nodes.reserve(topology.nodes.size());
    graph.edges.reserve(topology.edges.size());
    graph.schemaVersion = 1;

    result.id_map.reserve(topology.nodes.size());

    const auto placement_map = build_placement_map(layout);

    struct EdgeKeyHash
    {
        std::size_t operator()(const std::pair<std::size_t, std::size_t>& key) const noexcept
        {
            return std::hash<std::size_t>{}(key.first) ^ (std::hash<std::size_t>{}(key.second) << 1);
        }
    };

    std::unordered_set<std::pair<std::size_t, std::size_t>, EdgeKeyHash> portal_edges;
    portal_edges.reserve(topology.portals.size() * 2);
    for (const auto& portal : topology.portals)
    {
        portal_edges.emplace(portal.entry, portal.exit);
        portal_edges.emplace(portal.exit, portal.entry);
    }

    std::uint32_t next_id = 1;
    for (const auto& node : topology.nodes)
    {
        world::LocationNode world_node{};
        world_node.id = world::LocationId{next_id++};
        result.id_map.emplace(node.local_id, world_node.id);

        if (node.parent && result.id_map.contains(*node.parent))
        {
            world_node.parent = result.id_map.at(*node.parent);
        }
        else
        {
            world_node.parent = world::InvalidLocation;
        }

        world_node.name = !node.label.empty() ? node.label : ("node_" + std::to_string(node.local_id));
        world_node.kind = infer_kind(node);
        world_node.navigable = true;
        world_node.terrain = node.label;

        if (const auto placement_it = placement_map.find(node.local_id); placement_it != placement_map.end())
        {
            const auto& placement = placement_it->second;
            const auto x = static_cast<int>(std::lround(placement.x));
            const auto y = static_cast<int>(std::lround(placement.y));
            world_node.coord_global = std::make_pair(x, y);
        }

        graph.nodes.push_back(std::move(world_node));
    }

    for (const auto& edge : topology.edges)
    {
        const auto from_it = result.id_map.find(edge.from);
        const auto to_it = result.id_map.find(edge.to);
        if (from_it == result.id_map.end() || to_it == result.id_map.end())
        {
            continue;
        }

        world::PathEdge world_edge{};
        world_edge.from = from_it->second;
        world_edge.to = to_it->second;
        world_edge.cost = 1.0f;
        world_edge.bidirectional = edge.bidirectional;

        if (portal_edges.contains({edge.from, edge.to}))
        {
            world_edge.anchor_at_from = std::make_pair(0, 0);
            world_edge.anchor_at_to = std::make_pair(0, 0);
        }

        graph.edges.push_back(std::move(world_edge));
    }

    return result;
}
} // namespace

GeneratedWorld generate_world(const GeneratorConfig& config, Seed seed)
{
    WorldGenContext context(config, DeterministicRng(seed));
    context.log("世界生成开始");

    TopologyModule topology(context.config.topology);
    auto topology_rng = context.root_rng.fork(0x7A7A7A7Aull);
    auto draft = topology.generate(topology_rng);

    LayoutModule layout(context.config.layout);
    auto layout_rng = context.root_rng.fork(0x13579BDFull);
    auto layout_result = layout.generate(draft, layout_rng);

    ValidationModule validator;
    std::vector<ValidationError> validation_errors;
    if (!validator.validate(draft, layout_result, validation_errors))
    {
        std::ostringstream err;
        err << "世界生成校验失败:";
        for (const auto& error : validation_errors)
        {
            err << "\n - " << error.message;
        }
        throw std::runtime_error(err.str());
    }

    auto graph_result = build_location_graph(draft, layout_result);
    TilemapModule tilemap_module(context.config.tilemap);
    graph_result.graph.tilemaps = tilemap_module.generate(graph_result.id_map, layout_result);
    auto world_graph = std::move(graph_result.graph);

    std::ostringstream topo_log;
    topo_log << "拓扑生成: nodes=" << draft.nodes.size() << ", edges=" << draft.edges.size();
    context.log(topo_log.str());

    std::ostringstream layout_log;
    layout_log << "布局生成: placements=" << layout_result.placements.size();
    context.log(layout_log.str());

    context.log("世界生成完成");

    GeneratedWorld world{};
    world.seed = seed;
    world.location_count = world_graph.nodes.size();
    world.edge_count = world_graph.edges.size();
    world.logs = std::move(context.logs);
    world.topology = std::move(draft);
    world.layout = std::move(layout_result);
    world.world_graph = std::move(world_graph);

    return world;
}

} // namespace genesis::worldgen
