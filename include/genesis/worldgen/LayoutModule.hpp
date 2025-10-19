#pragma once

#include "genesis/worldgen/Rng.hpp"
#include "genesis/worldgen/Types.hpp"

namespace genesis::worldgen
{

class LayoutModule
{
public:
    explicit LayoutModule(LayoutSettings settings);

    [[nodiscard]] LayoutDraft generate(const TopologyDraft& topology, DeterministicRng& rng) const;

private:
    LayoutSettings settings_;
};

} // namespace genesis::worldgen
