#include "genesis/runtime/WorldAtlas.hpp"

#include <unordered_map>

#include "genesis/runtime/SchemaVersions.hpp"

namespace Genesis::Runtime {

namespace {

namespace world = Genesis::World;

using MapId = world::MapId;
using SceneId = world::SceneId;

[[nodiscard]] std::optional<std::pair<int, int>> resolveSceneOrigin(const world::Scene& scene) {
    if (scene.origin) {
        return scene.origin;
    }
    return std::nullopt;
}

} // namespace

WorldAtlas buildWorldAtlasFromDatabase(const world::WorldDatabase& db, std::uint32_t worldVersion) {
    WorldAtlas atlas{};
    atlas.schema_version = kWorldAtlasSchemaVersion;
    atlas.world_version = worldVersion;
    atlas.maps = db.maps();
    atlas.mapEdges = db.mapEdges();

    atlas.perMap.reserve(atlas.maps.size());
    for (const auto& map : atlas.maps) {
        WorldAtlas::PerMap per{};
        per.mapId = map.id;
        per.scenes = db.scenes(map.id);
        per.interactions = db.interactions(map.id);
        per.portals = db.portals(map.id);
        per.tilemap = db.tilemap(map.id);

        std::unordered_map<SceneId, std::pair<int, int>> originByScene;
        originByScene.reserve(per.scenes.size());
        for (const auto& scene : per.scenes) {
            if (auto origin = resolveSceneOrigin(scene)) {
                originByScene.emplace(scene.id, *origin);
            }
        }

        for (auto& interaction : per.interactions) {
            if (interaction.coordGlobal) {
                continue;
            }
            auto originIt = originByScene.find(interaction.sceneId);
            if (originIt == originByScene.end()) {
                continue;
            }
            const auto& origin = originIt->second;
            interaction.coordGlobal = std::make_pair(origin.first + interaction.coordLocal.first,
                                                    origin.second + interaction.coordLocal.second);
        }

        atlas.perMap.push_back(std::move(per));
    }

    return atlas;
}

} // namespace Genesis::Runtime
