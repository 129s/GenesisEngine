#include "genesis/world/WorldDatabaseLoader.hpp"

#include <fstream>
#include <nlohmann/json.hpp>

namespace genesis::world {

using nlohmann::json;

namespace {
    std::string readFile(const std::filesystem::path& p) {
        std::ifstream ifs(p, std::ios::binary);
        if (!ifs) return {};
        std::string s;
        ifs.seekg(0, std::ios::end);
        s.resize(static_cast<size_t>(ifs.tellg()));
        ifs.seekg(0, std::ios::beg);
        ifs.read(s.data(), static_cast<std::streamsize>(s.size()));
        return s;
    }

    InteractionKind parseInteractionKind(std::string_view sv) {
        if (sv == "Resource") return InteractionKind::Resource;
        if (sv == "Portal") return InteractionKind::Portal;
        if (sv == "Landmark") return InteractionKind::Landmark;
        return InteractionKind::Unknown;
    }
}

WorldDbLoadResult loadWorldDatabaseFromFolder(const std::filesystem::path& folder) {
    WorldDbLoadResult result{};

    const auto worldPath = folder / "world.json";
    auto worldText = readFile(worldPath);
    if (worldText.empty()) {
        result.success = false;
        result.error = "未找到 world.json 或为空: " + worldPath.string();
        return result;
    }

    json root;
    try {
        root = json::parse(worldText);
    } catch (const std::exception& e) {
        result.success = false;
        result.error = std::string("world.json 解析失败: ") + e.what();
        return result;
    }

    auto db = std::make_shared<InMemoryWorldDatabase>();

    // maps
    if (root.contains("maps") && root["maps"].is_array()) {
        for (const auto& jm : root["maps"]) {
            Map m{};
            m.id = jm.value("id", 0U);
            m.name = jm.value("name", std::string{});
            if (m.id != 0) db->addMap(std::move(m));
        }
    }

    // map_edges
    if (root.contains("map_edges") && root["map_edges"].is_array()) {
        for (const auto& je : root["map_edges"]) {
            MapEdge e{};
            e.from = je.value("from", 0U);
            e.to = je.value("to", 0U);
            e.bidirectional = je.value("bidirectional", true);
            if (e.from != 0 && e.to != 0) db->addMapEdge(std::move(e));
        }
    }

    // per-map detail files
    for (const auto& m : db->maps()) {
        const auto mapPath = folder / (std::string("map_") + std::to_string(m.id) + ".json");
        auto text = readFile(mapPath);
        if (text.empty()) {
            // 允许缺省
            continue;
        }
        json jm;
        try {
            jm = json::parse(text);
        } catch (const std::exception&) {
            // 忽略单图解析错误以保持容错
            continue;
        }

        if (jm.contains("scenes") && jm["scenes"].is_array()) {
            for (const auto& js : jm["scenes"]) {
                Scene s{};
                s.id = js.value("id", 0U);
                s.mapId = m.id;
                s.name = js.value("name", std::string{});
                if (s.id != 0) db->addScene(std::move(s));
            }
        }

        if (jm.contains("interactions") && jm["interactions"].is_array()) {
            for (const auto& ji : jm["interactions"]) {
                Interaction i{};
                i.id = ji.value("id", 0U);
                i.mapId = m.id;
                i.sceneId = ji.value("sceneId", 0U);
                i.name = ji.value("name", std::string{});
                i.kind = parseInteractionKind(ji.value("kind", std::string{"Unknown"}));
                if (ji.contains("coord") && ji["coord"].is_array() && ji["coord"].size() >= 2) {
                    i.coord.first = ji["coord"][0].get<int>();
                    i.coord.second = ji["coord"][1].get<int>();
                }
                if (ji.contains("capacity") && ji["capacity"].is_number_unsigned()) {
                    i.capacity = ji["capacity"].get<std::uint32_t>();
                }
                if (ji.contains("regen") && ji["regen"].is_number_unsigned()) {
                    i.regenPerStep = ji["regen"].get<std::uint32_t>();
                }
                if (i.id != 0) db->addInteraction(std::move(i));
            }
        }

        if (jm.contains("portals") && jm["portals"].is_array()) {
            for (const auto& jp : jm["portals"]) {
                Portal p{};
                p.interactionId = jp.value("interactionId", 0U);
                p.targetMapId = jp.value("targetMapId", 0U);
                if (jp.contains("targetSceneId")) p.targetSceneId = jp.value("targetSceneId", 0U);
                if (jp.contains("targetCoord") && jp["targetCoord"].is_array() && jp["targetCoord"].size() >= 2) {
                    p.targetCoord = std::make_pair(jp["targetCoord"][0].get<int>(), jp["targetCoord"][1].get<int>());
                }
                if (p.interactionId != 0 && p.targetMapId != 0) db->addPortal(std::move(p));
            }
        }
    }

    result.success = true;
    result.database = std::move(db);
    return result;
}

} // namespace genesis::world

