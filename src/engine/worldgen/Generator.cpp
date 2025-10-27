#include "genesis/worldgen/Generator.hpp"

#include <algorithm>
#include <cmath>
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
