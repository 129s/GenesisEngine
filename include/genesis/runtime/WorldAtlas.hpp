#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "genesis/runtime/SchemaVersions.hpp"
#include "genesis/world/WorldDatabase.hpp"

namespace Genesis::Runtime {

struct WorldAtlas {
    std::uint32_t schema_version{kWorldAtlasSchemaVersion};
    std::uint32_t world_version{0};

    struct PerMap {
        genesis::world::MapId mapId{0};
        std::vector<genesis::world::Scene> scenes;
        std::vector<genesis::world::Interaction> interactions;
        std::vector<genesis::world::Portal> portals;
        std::optional<genesis::world::TilemapMeta> tilemap;
    };

    std::vector<genesis::world::Map> maps;
    std::vector<genesis::world::MapEdge> mapEdges;
    std::vector<PerMap> perMap;
};

} // namespace Genesis::Runtime
