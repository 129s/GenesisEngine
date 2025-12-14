#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "genesis/world/WorldDatabase.hpp"

namespace genesis::world {

struct WorldDbLoadResult {
    bool success{false};
    std::string error;
    std::shared_ptr<WorldDatabase> database; // 成功时返回实例
};

// 从目录加载：
// - world.json:
//   - maps:      [{ id,name,meta? }]
//   - map_edges: [{ from,to,bidirectional?,cost?,rules? }]
// - map_#.json:
//   - scenes:       [{ id,parent?,name,origin?,transform?,meta? }]
//   - interactions: [{ id,sceneId,kind,coord_local,coord_global?,meta?,name,resourceType?,capacity?,regen? }]
//   - portals:      [{ interactionId,targetMapId,targetSceneId?,targetCoord?,channelId?,oneWay?,teleportCost? }]
//   - tilemap?:     { width,height,tileW,tileH,meta? }
WorldDbLoadResult loadWorldDatabaseFromFolder(const std::filesystem::path& folder);

} // namespace genesis::world

namespace Genesis {
namespace World = genesis::world;
} // namespace Genesis
