#include "genesis/world/WorldDatabaseSaver.hpp"

#include <fstream>
#include <nlohmann/json.hpp>

namespace genesis::world {

using nlohmann::json;

static const char* resourceTypeName(ResourceType type) {
    switch (type) {
    case ResourceType::Food:
        return "Food";
    case ResourceType::Drink:
        return "Drink";
    case ResourceType::Social:
        return "Social";
    }
    return "Food";
}

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
            json jm = {{"id", m.id}, {"name", m.name}};
            if (m.meta) {
                jm["meta"] = *m.meta;
            }
            world["maps"].push_back(std::move(jm));
        }
        world["map_edges"] = json::array();
        for (const auto& e : db.mapEdges()) {
            json je = {{"from", e.from}, {"to", e.to}, {"cost", e.cost}};
            if (e.bidirectional) {
                je["bidirectional"] = true;
            }
            if (e.rules) {
                je["rules"] = *e.rules;
            }
            world["map_edges"].push_back(std::move(je));
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
                json js = {{"id", s.id}, {"name", s.name}};
                if (s.parent) {
                    js["parent"] = *s.parent;
                }
                if (s.origin) {
                    js["origin"] = {s.origin->first, s.origin->second};
                }
                if (s.transform) {
                    js["transform"] = *s.transform;
                }
                if (s.meta) {
                    js["meta"] = *s.meta;
                }
                jm["scenes"].push_back(std::move(js));
            }
            // interactions
            jm["interactions"] = json::array();
            for (const auto& i : db.interactions(m.id)) {
                json ji = {
                    {"id", i.id},
                    {"sceneId", i.sceneId},
                    {"kind", (i.kind == InteractionKind::Resource ? "Resource" : (i.kind == InteractionKind::Portal ? "Portal" : (i.kind == InteractionKind::Landmark ? "Landmark" : "Unknown")))},
                    {"coord_local", {i.coordLocal.first, i.coordLocal.second}},
                    {"name", i.name}
                };
                if (i.coordGlobal) {
                    ji["coord_global"] = {i.coordGlobal->first, i.coordGlobal->second};
                }
                // legacy alias for compatibility
                const auto coord = i.worldCoord();
                ji["coord"] = {coord.first, coord.second};
                if (i.meta) {
                    ji["meta"] = *i.meta;
                }
                jm["interactions"].push_back(std::move(ji));
                if (i.kind == InteractionKind::Resource) {
                    if (i.resourceType) {
                        jm["interactions"].back()["resourceType"] = resourceTypeName(*i.resourceType);
                    }
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
                if (p.channelId) {
                    jp["channelId"] = *p.channelId;
                }
                if (p.oneWay) {
                    jp["oneWay"] = true;
                }
                if (p.teleportCost != 0.0) {
                    jp["teleportCost"] = p.teleportCost;
                }
                if (p.targetSceneId) jp["targetSceneId"] = *p.targetSceneId;
                if (p.targetCoord) jp["targetCoord"] = {p.targetCoord->first, p.targetCoord->second};
                jm["portals"].push_back(std::move(jp));
            }

            if (auto tile = db.tilemap(m.id)) {
                json jt = {
                    {"width", tile->width},
                    {"height", tile->height},
                    {"tileW", tile->tileW},
                    {"tileH", tile->tileH}
                };
                if (tile->meta) {
                    jt["meta"] = *tile->meta;
                }
                jm["tilemap"] = std::move(jt);
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
