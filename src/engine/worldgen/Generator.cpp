#include "genesis/worldgen/Generator.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <cstdint>
#include "genesis/worldgen/LayoutModule.hpp"
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

std::string resource_type_name(genesis::world::ResourceType type)
{
    switch (type)
    {
    case genesis::world::ResourceType::Food:
        return "Food";
    case genesis::world::ResourceType::Drink:
        return "Drink";
    case genesis::world::ResourceType::Social:
        return "Social";
    }
    return "Food";
}

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

std::size_t next_local_id(const TopologyDraft& topology)
{
    std::size_t next = 0;
    for (const auto& node : topology.nodes)
    {
        next = std::max(next, node.local_id + 1);
    }
    return next;
}

void add_interactive_resources(TopologyDraft& topology, const GeneratorConfig& config)
{
    if (config.worlddb.resources.per_map == 0)
    {
        return;
    }

    std::size_t id = next_local_id(topology);
    const auto typeTag = std::string("resource_type=") + resource_type_name(config.worlddb.resources.type);

    // 注意：只给 Scene 生成子资源节点
    const auto original_count = topology.nodes.size();
    for (std::size_t i = 0; i < original_count; ++i)
    {
        const auto& node = topology.nodes[i];
        if (node.kind != DraftNodeKind::Scene)
        {
            continue;
        }

        const auto count = config.worlddb.resources.per_map;
        for (std::size_t j = 0; j < count; ++j)
        {
            NodeDraft resource{};
            resource.local_id = id++;
            resource.kind = DraftNodeKind::InteractiveResource;
            resource.label = (count == 1) ? "resource" : ("resource_" + std::to_string(j));
            resource.parent = node.local_id;
            resource.tags = {"resource", typeTag};
            topology.nodes.push_back(std::move(resource));
        }
    }
}

void add_interactive_portals(TopologyDraft& topology)
{
    std::size_t id = next_local_id(topology);

    for (std::size_t index = 0; index < topology.portals.size(); ++index)
    {
        const auto& portal = topology.portals[index];
        if (portal.entry == portal.exit)
        {
            continue;
        }

        NodeDraft portal_node{};
        portal_node.local_id = id++;
        portal_node.kind = DraftNodeKind::InteractivePortal;
        portal_node.label = "portal";
        portal_node.parent = portal.entry;
        portal_node.tags = {"portal", "portal_target=" + std::to_string(portal.exit)};
        topology.nodes.push_back(std::move(portal_node));
    }
}

} // namespace

GeneratedWorld generate_world(const GeneratorConfig& config, Seed seed)
{
    WorldGenContext context(config, DeterministicRng(seed));
    context.log("世界生成开始");

    TopologyModule topology(context.config.topology);
    auto topology_rng = context.root_rng.fork(0x7A7A7A7Aull);
    auto draft = topology.generate(topology_rng);

    // v2: 互动节点也作为拓扑的一部分，供 Layout/WorldDB 使用（不再生成旧 LocationGraph）。
    add_interactive_resources(draft, config);
    add_interactive_portals(draft);

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

    // v2: 不再构建旧 LocationGraph；保持 Topology + Layout 输出供后续数据库生成阶段使用。

    std::ostringstream topo_log;
    topo_log << "拓扑生成: nodes=" << draft.nodes.size() << ", edges=" << draft.edges.size();
    context.log(topo_log.str());

    std::ostringstream layout_log;
    layout_log << "布局生成: placements=" << layout_result.placements.size();
    context.log(layout_log.str());

    // v2: Tilemap 生成与渲染解耦，后续在可视化阶段处理。

    context.log("世界生成完成");

    GeneratedWorld world{};
    world.seed = seed;
    world.location_count = draft.nodes.size();
    world.edge_count = draft.edges.size();
    world.logs = std::move(context.logs);
    world.topology = std::move(draft);
    world.layout = std::move(layout_result);

    return world;
}

} // namespace genesis::worldgen
