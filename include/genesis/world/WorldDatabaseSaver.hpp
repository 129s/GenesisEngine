#pragma once

#include <filesystem>
#include <string>

#include "genesis/world/WorldDatabase.hpp"

namespace genesis::world {

struct WorldDbSaveResult {
    bool success{false};
    std::string error;
};

// 将内存 DB 保存为文件夹：world.json + map_#.json
WorldDbSaveResult saveWorldDatabaseToFolder(const std::filesystem::path& folder, const WorldDatabase& db);

} // namespace genesis::world

namespace Genesis {
namespace World = genesis::world;
} // namespace Genesis
