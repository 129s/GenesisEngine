#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "genesis/runtime/SchemaVersions.hpp"
#include "genesis/world/Namespace.hpp"
#include "genesis/world/WorldDatabase.hpp"

namespace Genesis::Runtime {

namespace world = Genesis::World;

struct WorldAtlas {
    std::uint32_t schema_version{kWorldAtlasSchemaVersion};
    std::uint32_t world_version{0};

    struct PerMap {
        world::MapId mapId{0};
        std::vector<world::Scene> scenes;
        std::vector<world::Interaction> interactions;
        std::vector<world::Portal> portals;
        std::optional<world::TilemapMeta> tilemap;
    };

    std::vector<world::Map> maps;
    std::vector<world::MapEdge> mapEdges;
    std::vector<PerMap> perMap;
};

} // namespace Genesis::Runtime
