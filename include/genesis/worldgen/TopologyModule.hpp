#pragma once

#include "genesis/worldgen/Rng.hpp"
#include "genesis/worldgen/Types.hpp"

namespace genesis::worldgen
{

class TopologyModule
{
public:
    explicit TopologyModule(TopologySettings settings);

    [[nodiscard]] TopologyDraft generate(DeterministicRng& rng) const;

private:
    TopologySettings settings_;

    [[nodiscard]] std::size_t random_inclusive(DeterministicRng& rng, std::size_t min, std::size_t max) const;
};

} // namespace genesis::worldgen

