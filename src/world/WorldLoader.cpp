#include "genesis/world/WorldLoader.hpp"

#include <fstream>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace genesis::world {

namespace {

constexpr auto kIndentWidth = 2;

LocationKind parseLocationKind(const std::string& value, bool& ok) {
    static const std::unordered_map<std::string, LocationKind> map{
        {"Region", LocationKind::Region},
        {"Building", LocationKind::Building},
        {"Room", LocationKind::Room},
        {"Point", LocationKind::Point},
    };

    if (auto it = map.find(value); it != map.end()) {
        ok = true;
        return it->second;
    }

    ok = false;
    return LocationKind::Point;
}

ResourceType parseResourceType(const std::string& value, bool& ok) {
    static const std::unordered_map<std::string, ResourceType> map{
        {"Food", ResourceType::Food},
        {"Drink", ResourceType::Drink},
        {"Social", ResourceType::Social},
    };

    if (auto it = map.find(value); it != map.end()) {
        ok = true;
        return it->second;
    }

    ok = false;
    return ResourceType::Food;
}

std::string_view locationKindToString(LocationKind kind) {
    switch (kind) {
    case LocationKind::Region:
        return "Region";
    case LocationKind::Building:
        return "Building";
    case LocationKind::Room:
        return "Room";
    case LocationKind::Point:
    default:
        return "Point";
    }
}

std::string_view resourceTypeToString(ResourceType type) {
    switch (type) {
    case ResourceType::Food:
        return "Food";
    case ResourceType::Drink:
        return "Drink";
    case ResourceType::Social:
        return "Social";
    default:
        return "Food";
    }
}

WorldGraphLoadResult buildGraphFromJson(const nlohmann::json& data) {
    WorldGraphLoadResult result{};
    LocationGraph graph;

    if (data.contains("schema_version")) {
        try {
            graph.schemaVersion = data.at("schema_version").get<std::uint32_t>();
        } catch (const nlohmann::json::exception&) {
            // ignore malformed schema_version; keep default 0
        }
    }

    if (!data.contains("locations") || !data["locations"].is_array()) {
        result.error = "Missing 'locations' array";
        return result;
    }

    for (const auto& nodeJson : data["locations"]) {
        LocationNode node{};
        try {
            node.id = LocationId{nodeJson.at("id").get<std::uint32_t>()};
            node.parent = LocationId{nodeJson.value("parent", 0U)};
            node.name = nodeJson.value("name", std::string{});
            node.navigable = nodeJson.value("navigable", true);
            node.terrain = nodeJson.value("terrain", std::string{});

            bool kindOk = true;
            node.kind = parseLocationKind(nodeJson.value("kind", std::string{"Point"}), kindOk);
            if (!kindOk) {
                result.error = "Unknown location kind: " + nodeJson.value("kind", std::string{});
                return result;
            }

            if (nodeJson.contains("coord_global")) {
                const auto& cg = nodeJson.at("coord_global");
                if (cg.is_array() && cg.size() == 2) {
                    node.coord_global = std::make_pair(cg[0].get<int>(), cg[1].get<int>());
                }
            }
        } catch (const nlohmann::json::exception& ex) {
            result.error = std::string{"Invalid location entry: "} + ex.what();
            return result;
        }
        graph.nodes.push_back(std::move(node));
    }

    if (data.contains("edges")) {
        if (!data["edges"].is_array()) {
            result.error = "'edges' must be an array";
            return result;
        }

        for (const auto& edgeJson : data["edges"]) {
            PathEdge edge{};
            try {
                edge.from = LocationId{edgeJson.at("from").get<std::uint32_t>()};
                edge.to = LocationId{edgeJson.at("to").get<std::uint32_t>()};
                edge.cost = edgeJson.value("cost", 1.0f);
                edge.bidirectional = edgeJson.value("bidirectional", true);

                if (edgeJson.contains("anchors")) {
                    const auto& anchors = edgeJson.at("anchors");
                    auto parseCoord = [](const nlohmann::json& arr) -> std::optional<std::pair<int, int>> {
                        if (!arr.is_array() || arr.size() != 2) {
                            return std::nullopt;
                        }
                        return std::make_pair(arr[0].get<int>(), arr[1].get<int>());
                    };
                    if (anchors.contains("at_from")) {
                        edge.anchor_at_from = parseCoord(anchors.at("at_from"));
                    }
                    if (anchors.contains("at_to")) {
                        edge.anchor_at_to = parseCoord(anchors.at("at_to"));
                    }
                }

                if (edgeJson.contains("polyline") && edgeJson.at("polyline").is_array()) {
                    for (const auto& pt : edgeJson.at("polyline")) {
                        if (pt.is_array() && pt.size() == 2) {
                            edge.polyline.emplace_back(pt[0].get<int>(), pt[1].get<int>());
                        }
                    }
                }
            } catch (const nlohmann::json::exception& ex) {
                result.error = std::string{"Invalid edge entry: "} + ex.what();
                return result;
            }
            graph.edges.push_back(std::move(edge));
        }
    }

    if (data.contains("spawns")) {
        if (!data["spawns"].is_array()) {
            result.error = "'spawns' must be an array";
            return result;
        }

        for (const auto& spawnJson : data["spawns"]) {
            ResourceSpawn spawn{};
            try {
                spawn.name = spawnJson.value("name", std::string{});
                spawn.location = LocationId{spawnJson.at("location").get<std::uint32_t>()};
                spawn.capacity = spawnJson.value("capacity", 0U);
                spawn.ratePerStep = spawnJson.value("rate_per_step", 0U);

                bool typeOk = true;
                spawn.type = parseResourceType(spawnJson.value("type", std::string{"Food"}), typeOk);
                if (!typeOk) {
                    result.error = "Unknown resource type: " + spawnJson.value("type", std::string{});
                    return result;
                }

                if (spawnJson.contains("local_coord")) {
                    const auto& lc = spawnJson.at("local_coord");
                    if (lc.is_array() && lc.size() == 2) {
                        spawn.local_coord = std::make_pair(lc[0].get<int>(), lc[1].get<int>());
                    }
                }
            } catch (const nlohmann::json::exception& ex) {
                result.error = std::string{"Invalid spawn entry: "} + ex.what();
                return result;
            }
            graph.spawns.push_back(std::move(spawn));
        }
    }

    if (data.contains("tilemaps")) {
        if (!data["tilemaps"].is_array()) {
            result.error = "'tilemaps' must be an array";
            return result;
        }
        for (const auto& tm : data["tilemaps"]) {
            try {
                TilemapMeta meta{};
                meta.node = LocationId{tm.at("node").get<std::uint32_t>()};
                meta.width = tm.value("width", 0);
                meta.height = tm.value("height", 0);
                const int s = tm.value("tileSize", 0);
                meta.tileW = s;
                meta.tileH = s;
                graph.tilemaps.push_back(meta);
            } catch (const nlohmann::json::exception& ex) {
                result.error = std::string{"Invalid tilemap entry: "} + ex.what();
                return result;
            }
        }
    }

    result.success = true;
    result.graph = std::move(graph);
    return result;
}

} // namespace

WorldGraphLoadResult loadWorldGraphFromJsonString(std::string_view jsonData) {
    try {
        auto data = nlohmann::json::parse(jsonData);
        return buildGraphFromJson(data);
    } catch (const nlohmann::json::exception& ex) {
        return {false, std::string{"Failed to parse JSON: "} + ex.what(), {}};
    }
}

WorldGraphLoadResult loadWorldGraphFromFile(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return {false, "Unable to open world file: " + path.string(), {}};
    }

    try {
        nlohmann::json data;
        stream >> data;
        return buildGraphFromJson(data);
    } catch (const nlohmann::json::exception& ex) {
        return {false, std::string{"Failed to parse JSON: "} + ex.what(), {}};
    }
}

WorldLoadResult loadWorldFromJsonString(std::string_view jsonData, WorldRegistry& registry) {
    auto result = loadWorldGraphFromJsonString(jsonData);
    if (!result.success) {
        return {false, std::move(result.error)};
    }

    registry.setGraph(std::move(result.graph));
    return {true, {}};
}

WorldLoadResult loadWorldFromFile(const std::filesystem::path& path, WorldRegistry& registry) {
    auto result = loadWorldGraphFromFile(path);
    if (!result.success) {
        return {false, std::move(result.error)};
    }

    registry.setGraph(std::move(result.graph));
    return {true, {}};
}

WorldSaveResult saveWorldToFile(const std::filesystem::path& path, const LocationGraph& graph) {
    nlohmann::json data;
    data["schema_version"] = graph.schemaVersion;

    data["locations"] = nlohmann::json::array();
    for (const auto& node : graph.nodes) {
        nlohmann::json nodeJson;
        nodeJson["id"] = node.id.value;
        nodeJson["parent"] = node.parent.value;
        nodeJson["name"] = node.name;
        nodeJson["navigable"] = node.navigable;
        nodeJson["terrain"] = node.terrain;
        nodeJson["kind"] = locationKindToString(node.kind);
        if (node.coord_global.has_value()) {
            nodeJson["coord_global"] = {node.coord_global->first, node.coord_global->second};
        }
        data["locations"].push_back(std::move(nodeJson));
    }

    if (!graph.edges.empty()) {
        data["edges"] = nlohmann::json::array();
        for (const auto& edge : graph.edges) {
            nlohmann::json edgeJson;
            edgeJson["from"] = edge.from.value;
            edgeJson["to"] = edge.to.value;
            edgeJson["cost"] = edge.cost;
            edgeJson["bidirectional"] = edge.bidirectional;

            nlohmann::json anchors;
            if (edge.anchor_at_from.has_value()) {
                anchors["at_from"] = {edge.anchor_at_from->first, edge.anchor_at_from->second};
            }
            if (edge.anchor_at_to.has_value()) {
                anchors["at_to"] = {edge.anchor_at_to->first, edge.anchor_at_to->second};
            }
            if (!anchors.empty()) {
                edgeJson["anchors"] = std::move(anchors);
            }

            if (!edge.polyline.empty()) {
                nlohmann::json poly = nlohmann::json::array();
                for (const auto& pt : edge.polyline) {
                    poly.push_back({pt.first, pt.second});
                }
                edgeJson["polyline"] = std::move(poly);
            }

            data["edges"].push_back(std::move(edgeJson));
        }
    }

    if (!graph.spawns.empty()) {
        data["spawns"] = nlohmann::json::array();
        for (const auto& spawn : graph.spawns) {
            nlohmann::json spawnJson;
            spawnJson["name"] = spawn.name;
            spawnJson["location"] = spawn.location.value;
            spawnJson["capacity"] = spawn.capacity;
            spawnJson["rate_per_step"] = spawn.ratePerStep;
            spawnJson["type"] = resourceTypeToString(spawn.type);
            if (spawn.local_coord.has_value()) {
                spawnJson["local_coord"] = {spawn.local_coord->first, spawn.local_coord->second};
            }
            data["spawns"].push_back(std::move(spawnJson));
        }
    }

    if (!graph.tilemaps.empty()) {
        data["tilemaps"] = nlohmann::json::array();
        for (const auto& tm : graph.tilemaps) {
            nlohmann::json tmJson;
            tmJson["node"] = tm.node.value;
            tmJson["width"] = tm.width;
            tmJson["height"] = tm.height;
            const int tileSize = tm.tileW > 0 ? tm.tileW : tm.tileH;
            if (tileSize > 0) {
                tmJson["tileSize"] = tileSize;
            }
            data["tilemaps"].push_back(std::move(tmJson));
        }
    }

    try {
        if (!path.parent_path().empty()) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream stream(path, std::ios::trunc);
        if (!stream.is_open()) {
            return {false, "Unable to open path for writing: " + path.string()};
        }
        stream << data.dump(kIndentWidth) << std::endl;
    } catch (const std::exception& ex) {
        return {false, std::string{"Failed to write world file: "} + ex.what()};
    }

    return {true, {}};
}

} // namespace genesis::world
