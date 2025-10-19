#include "genesis/worldgen/TilemapModule.hpp"

#include <cmath>
#include <unordered_map>

namespace genesis::worldgen
{

TilemapModule::TilemapModule(TilemapSettings settings)
    : settings_(std::move(settings))
{
    if (settings_.tile_size <= 0)
    {
        settings_.tile_size = 1;
    }
    if (settings_.base_extent <= 0)
    {
        settings_.base_extent = 16;
    }
}

std::vector<world::TilemapMeta> TilemapModule::generate(
    const std::unordered_map<std::size_t, world::LocationId>& id_map,
    const LayoutDraft& layout) const
{
    std::vector<world::TilemapMeta> tilemaps;
    tilemaps.reserve(layout.placements.size());

    for (const auto& placement : layout.placements)
    {
        const auto it = id_map.find(placement.local_id);
        if (it == id_map.end())
        {
            continue;
        }

        world::TilemapMeta meta{};
        meta.node = it->second;
        meta.tileW = settings_.tile_size;
        meta.tileH = settings_.tile_size;
        meta.width = settings_.base_extent;
        meta.height = settings_.base_extent;
        tilemaps.push_back(meta);
    }

    return tilemaps;
}

} // namespace genesis::worldgen

