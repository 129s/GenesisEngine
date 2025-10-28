#include "genesis/world/WorldDatabaseSaver.hpp"

#include <fstream>
#include <nlohmann/json.hpp>

namespace genesis::world {

using nlohmann::json;

static bool writeText(const std::filesystem::path& p, const std::string& s) {
    std::filesystem::create_directories(p.parent_path());
    std::ofstream ofs(p, std::ios::binary);
    if (!ofs) return false;
    ofs << s;
    return true;
}

WorldDbSaveResult saveWorldDatabaseToFolder(const std::filesystem::path& folder, const WorldDatabase& db) {
    WorldDbSaveResult result{};
    try {
        // world.json
        json world;
        world["maps"] = json::array();
        for (const auto& m : db.maps()) {
            world["maps"].push_back({{"id", m.id}, {"name", m.name}});
        }
        world["map_edges"] = json::array();
        for (const auto& e : db.mapEdges()) {
            world["map_edges"].push_back({{"from", e.from}, {"to", e.to}, {"bidirectional", e.bidirectional}});
        }
        if (!writeText(folder / "world.json", world.dump(2))) {
            result.success = false;
            result.error = "无法写入 world.json";
            return result;
        }

        // map_#.json
        for (const auto& m : db.maps()) {
            json jm;
            // scenes
            jm["scenes"] = json::array();
            for (const auto& s : db.scenes(m.id)) {
                jm["scenes"].push_back({{"id", s.id}, {"name", s.name}});
            }
            // interactions
            jm["interactions"] = json::array();
            for (const auto& i : db.interactions(m.id)) {
                jm["interactions"].push_back({
                    {"id", i.id},
                    {"sceneId", i.sceneId},
                    {"kind", (i.kind == InteractionKind::Resource ? "Resource" : (i.kind == InteractionKind::Portal ? "Portal" : (i.kind == InteractionKind::Landmark ? "Landmark" : "Unknown")))},
                    {"coord", {i.coord.first, i.coord.second}},
                    {"name", i.name}
                });
                if (i.kind == InteractionKind::Resource) {
                    if (i.capacity) {
                        jm["interactions"].back()["capacity"] = *i.capacity;
                    }
                    if (i.regenPerStep) {
                        jm["interactions"].back()["regen"] = *i.regenPerStep;
                    }
                }
            }
            // portals
            jm["portals"] = json::array();
            for (const auto& p : db.portals(m.id)) {
                json jp = {{"interactionId", p.interactionId}, {"targetMapId", p.targetMapId}};
                if (p.targetSceneId) jp["targetSceneId"] = *p.targetSceneId;
                if (p.targetCoord) jp["targetCoord"] = {p.targetCoord->first, p.targetCoord->second};
                jm["portals"].push_back(std::move(jp));
            }

            const auto path = folder / (std::string("map_") + std::to_string(m.id) + ".json");
            if (!writeText(path, jm.dump(2))) {
                result.success = false;
                result.error = std::string("无法写入 ") + path.string();
                return result;
            }
        }

        result.success = true;
        return result;
    } catch (const std::exception& e) {
        result.success = false;
        result.error = e.what();
        return result;
    }
}

} // namespace genesis::world

