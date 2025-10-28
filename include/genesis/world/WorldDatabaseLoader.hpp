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
// - world.json: { maps:[{id,name}], map_edges:[{from,to,bidirectional}] }
// - map_#.json: { scenes:[{id,name}], interactions:[{id,sceneId,kind,coord:[x,y],name}], portals:[{interactionId,targetMapId,targetSceneId?,targetCoord?}] }
WorldDbLoadResult loadWorldDatabaseFromFolder(const std::filesystem::path& folder);

} // namespace genesis::world

namespace Genesis {
namespace World = genesis::world;
} // namespace Genesis
