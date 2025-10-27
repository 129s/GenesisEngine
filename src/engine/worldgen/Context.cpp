#include "genesis/worldgen/Context.hpp"

#include <string_view>

namespace genesis::worldgen
{

WorldGenContext::WorldGenContext(GeneratorConfig cfg, DeterministicRng rng)
    : config(std::move(cfg))
    , root_rng(std::move(rng))
{
    logs.reserve(8);
}

void WorldGenContext::log(std::string_view message)
{
    logs.push_back(GenerationLogEntry{std::string(message)});
}

} // namespace genesis::worldgen

