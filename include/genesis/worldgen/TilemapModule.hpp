#pragma once

#include <unordered_map>
#include <vector>

#include "genesis/world/WorldTypes.hpp"
#include "genesis/worldgen/Types.hpp"

namespace genesis::worldgen
{

class TilemapModule
{
public:
    explicit TilemapModule(TilemapSettings settings);

    [[nodiscard]] std::vector<world::TilemapMeta> generate(
        const std::unordered_map<std::size_t, world::LocationId>& id_map,
        const LayoutDraft& layout) const;

private:
    TilemapSettings settings_;
};

} // namespace genesis::worldgen

