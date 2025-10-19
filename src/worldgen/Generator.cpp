#include "genesis/worldgen/Generator.hpp"

#include <sstream>

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

    std::ostringstream oss;
    oss << "拓扑生成: nodes=" << draft.nodes.size() << ", edges=" << draft.edges.size();
    context.log(oss.str());

    context.log("世界生成完成");

    GeneratedWorld world{};
    world.seed = seed;
    world.location_count = draft.nodes.size();
    world.edge_count = draft.edges.size();
    world.logs = std::move(context.logs);

    return world;
}

} // namespace genesis::worldgen
