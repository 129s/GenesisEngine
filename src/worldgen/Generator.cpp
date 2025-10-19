#include "genesis/worldgen/Generator.hpp"

#include <sstream>

#include "genesis/worldgen/LayoutModule.hpp"
#include "genesis/worldgen/TopologyModule.hpp"

namespace genesis::worldgen
{

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

    std::ostringstream topo_log;
    topo_log << "拓扑生成: nodes=" << draft.nodes.size() << ", edges=" << draft.edges.size();
    context.log(topo_log.str());

    std::ostringstream layout_log;
    layout_log << "布局生成: placements=" << layout_result.placements.size();
    context.log(layout_log.str());

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
