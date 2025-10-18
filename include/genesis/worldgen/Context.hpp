#pragma once

#include <string_view>
#include <utility>
#include <vector>

#include "genesis/worldgen/Rng.hpp"
#include "genesis/worldgen/Types.hpp"

namespace genesis::worldgen
{

struct WorldGenContext
{
    GeneratorConfig config;
    DeterministicRng root_rng;
    std::vector<GenerationLogEntry> logs;

    WorldGenContext(GeneratorConfig cfg, DeterministicRng rng);

    void log(std::string_view message);
};

} // namespace genesis::worldgen
