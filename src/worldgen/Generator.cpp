#include "genesis/worldgen/Generator.hpp"

#include <sstream>

namespace genesis::worldgen
{

GeneratedWorld generate_world(const GeneratorConfig& config, Seed seed)
{
    WorldGenContext context(config, DeterministicRng(seed));
    context.log("世界生成开始");

    auto metrics_rng = context.root_rng.fork(0xCAFEBABEull);
    const auto location_count = metrics_rng.uniform_index(5) + 1; // 至少 1 个节点
    const auto edge_count = location_count > 0 ? location_count - 1 : 0;

    std::ostringstream oss;
    oss << "生成统计: nodes=" << location_count << ", edges=" << edge_count;
    context.log(oss.str());
    context.log("世界生成完成");

    GeneratedWorld world{};
    world.seed = seed;
    world.location_count = location_count;
    world.edge_count = edge_count;
    world.logs = std::move(context.logs);

    return world;
}

} // namespace genesis::worldgen

