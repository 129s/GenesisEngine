#pragma once

#include "genesis/worldgen/Context.hpp"

namespace genesis::worldgen
{

GeneratedWorld generate_world(const GeneratorConfig& config, Seed seed);

} // namespace genesis::worldgen

