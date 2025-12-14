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

    std::optional<ResourceType> parseResourceType(std::string_view sv) {
        if (sv == "Food") return ResourceType::Food;
        if (sv == "Drink") return ResourceType::Drink;
        if (sv == "Social") return ResourceType::Social;
        return std::nullopt;
    }

    std::optional<std::pair<int, int>> parseInt2(const json& value) {
        if (!value.is_array() || value.size() < 2) {
            return std::nullopt;
        }
        return std::make_pair(value.at(0).get<int>(), value.at(1).get<int>());
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
            if (jm.contains("meta")) {
                m.meta = jm.at("meta");
            }
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
            e.cost = je.value("cost", 1.0);
            if (je.contains("rules")) {
                e.rules = je.at("rules");
            }
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
                if (js.contains("parent")) {
                    const auto parent = js.value("parent", 0U);
                    if (parent != 0U) {
                        s.parent = parent;
                    }
                }
                s.name = js.value("name", std::string{});
                if (js.contains("origin")) {
                    s.origin = parseInt2(js.at("origin"));
                }
                if (js.contains("transform")) {
                    s.transform = js.at("transform");
                }
                if (js.contains("meta")) {
                    s.meta = js.at("meta");
                }
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
                if (ji.contains("coord_local")) {
                    if (auto parsed = parseInt2(ji.at("coord_local"))) {
                        i.coordLocal = *parsed;
                    }
                } else if (ji.contains("coord")) { // legacy field
                    if (auto parsed = parseInt2(ji.at("coord"))) {
                        i.coordLocal = *parsed;
                        i.coordGlobal = *parsed;
                    }
                }
                if (ji.contains("coord_global")) {
                    if (auto parsed = parseInt2(ji.at("coord_global"))) {
                        i.coordGlobal = *parsed;
                    }
                }
                if (ji.contains("meta")) {
                    i.meta = ji.at("meta");
                }
                if (i.kind == InteractionKind::Resource) {
                    if (ji.contains("resourceType") && ji["resourceType"].is_string()) {
                        i.resourceType = parseResourceType(ji["resourceType"].get<std::string>());
                    } else if (ji.contains("type") && ji["type"].is_string()) {
                        i.resourceType = parseResourceType(ji["type"].get<std::string>());
                    }
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
                p.mapId = m.id;
                p.interactionId = jp.value("interactionId", 0U);
                p.targetMapId = jp.value("targetMapId", 0U);
                if (jp.contains("channelId") && jp["channelId"].is_string()) {
                    p.channelId = jp["channelId"].get<std::string>();
                }
                if (jp.contains("oneWay") && jp["oneWay"].is_boolean()) {
                    p.oneWay = jp["oneWay"].get<bool>();
                }
                if (jp.contains("teleportCost") && jp["teleportCost"].is_number()) {
                    p.teleportCost = jp["teleportCost"].get<double>();
                }
                if (jp.contains("targetSceneId")) {
                    const auto scene = jp.value("targetSceneId", 0U);
                    if (scene != 0U) {
                        p.targetSceneId = scene;
                    }
                }
                if (jp.contains("targetCoord")) {
                    if (auto parsed = parseInt2(jp.at("targetCoord"))) {
                        p.targetCoord = *parsed;
                    }
                }
                if (p.interactionId != 0 && p.targetMapId != 0) db->addPortal(std::move(p));
            }
        }

        if (jm.contains("tilemap") && jm["tilemap"].is_object()) {
            const auto& tile = jm["tilemap"];
            TilemapMeta meta{};
            meta.width = tile.value("width", 0);
            meta.height = tile.value("height", 0);
            meta.tileW = tile.value("tileW", tile.value("tile_size", 1));
            meta.tileH = tile.value("tileH", tile.value("tile_size", 1));
            if (tile.contains("meta")) {
                meta.meta = tile.at("meta");
            }
            if (meta.width > 0 && meta.height > 0) {
                db->setTilemap(m.id, std::move(meta));
            }
        }
    }

    result.success = true;
    result.database = std::move(db);
    return result;
}

} // namespace genesis::world
