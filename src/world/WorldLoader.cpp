#include "genesis/world/WorldLoader.hpp"

#include <fstream>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace genesis::world {

namespace {

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

WorldLoadResult loadGraphFromJson(const nlohmann::json& data, WorldRegistry& registry) {
    LocationGraph graph;
    if (data.contains("schema_version")) {
        try {
            graph.schemaVersion = data.at("schema_version").get<std::uint32_t>();
        } catch (const nlohmann::json::exception&) {
            // ignore malformed schema_version; keep default 0
        }
    }

    if (!data.contains("locations") || !data["locations"].is_array()) {
        return {false, "Missing 'locations' array"};
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
                return {false, "Unknown location kind: " + nodeJson.value("kind", std::string{})};
            }

            // Enforce coord_global presence
            if (!nodeJson.contains("coord_global")) {
                return {false, "Missing required 'coord_global' for location id=" + std::to_string(node.id.value)};
            }
            const auto& cg = nodeJson.at("coord_global");
            if (!cg.is_array() || cg.size() != 2) {
                return {false, "Invalid 'coord_global' format for location id=" + std::to_string(node.id.value)};
            }
            node.coord_global = std::make_pair(cg[0].get<int>(), cg[1].get<int>());
        } catch (const nlohmann::json::exception& ex) {
            return {false, std::string{"Invalid location entry: "} + ex.what()};
        }
        graph.nodes.push_back(std::move(node));
    }

    if (data.contains("edges")) {
        if (!data["edges"].is_array()) {
            return {false, "'edges' must be an array"};
        }

        for (const auto& edgeJson : data["edges"]) {
            PathEdge edge{};
            try {
                edge.from = LocationId{edgeJson.at("from").get<std::uint32_t>()};
                edge.to = LocationId{edgeJson.at("to").get<std::uint32_t>()};
                edge.cost = edgeJson.value("cost", 1.0f);
                edge.bidirectional = edgeJson.value("bidirectional", true);
                // anchors: { at_from:[x,y], at_to:[x,y] }
                if (edgeJson.contains("anchors")) {
                    const auto& anchors = edgeJson.at("anchors");
                    auto parseCoord = [](const nlohmann::json& arr) -> std::optional<std::pair<int, int>> {
                        if (!arr.is_array() || arr.size() != 2) return std::nullopt;
                        return std::make_pair(arr[0].get<int>(), arr[1].get<int>());
                    };
                    if (anchors.contains("at_from")) {
                        edge.anchor_at_from = parseCoord(anchors.at("at_from"));
                    }
                    if (anchors.contains("at_to")) {
                        edge.anchor_at_to = parseCoord(anchors.at("at_to"));
                    }
                }
                // polyline: [[x,y], ...]
                if (edgeJson.contains("polyline") && edgeJson.at("polyline").is_array()) {
                    for (const auto& pt : edgeJson.at("polyline")) {
                        if (pt.is_array() && pt.size() == 2) {
                            edge.polyline.emplace_back(pt[0].get<int>(), pt[1].get<int>());
                        }
                    }
                }
                // Enforce anchors presence
                if (!edgeJson.contains("anchors")) {
                    return {false, "Missing required 'anchors' for edge from=" + std::to_string(edge.from.value) + " to=" + std::to_string(edge.to.value)};
                }
                {
                    const auto& anchors = edgeJson.at("anchors");
                    auto parseCoord = [](const nlohmann::json& arr) -> std::optional<std::pair<int, int>> {
                        if (!arr.is_array() || arr.size() != 2) return std::nullopt;
                        return std::make_pair(arr[0].get<int>(), arr[1].get<int>());
                    };
                    if (!anchors.contains("at_from") || !anchors.contains("at_to")) {
                        return {false, "anchors must contain 'at_from' and 'at_to' for edge from=" + std::to_string(edge.from.value) + " to=" + std::to_string(edge.to.value)};
                    }
                    edge.anchor_at_from = parseCoord(anchors.at("at_from"));
                    edge.anchor_at_to = parseCoord(anchors.at("at_to"));
                    if (!edge.anchor_at_from.has_value() || !edge.anchor_at_to.has_value()) {
                        return {false, "Invalid anchors format for edge from=" + std::to_string(edge.from.value) + " to=" + std::to_string(edge.to.value)};
                    }
                }
                // optional polyline
                if (edgeJson.contains("polyline") && edgeJson.at("polyline").is_array()) {
                    for (const auto& pt : edgeJson.at("polyline")) {
                        if (pt.is_array() && pt.size() == 2) {
                            edge.polyline.emplace_back(pt[0].get<int>(), pt[1].get<int>());
                        }
                    }
                }
            } catch (const nlohmann::json::exception& ex) {
                return {false, std::string{"Invalid edge entry: "} + ex.what()};
            }

            graph.edges.push_back(edge);
        }
    }

    if (data.contains("spawns")) {
        if (!data["spawns"].is_array()) {
            return {false, "'spawns' must be an array"};
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
                    return {false, "Unknown resource type: " + spawnJson.value("type", std::string{})};
                }
                if (spawnJson.contains("local_coord")) {
                    const auto& lc = spawnJson.at("local_coord");
                    if (lc.is_array() && lc.size() == 2) {
                        spawn.local_coord = std::make_pair(lc[0].get<int>(), lc[1].get<int>());
                    }
                }
                // Enforce local_coord presence
                if (!spawnJson.contains("local_coord")) {
                    return {false, "Missing required 'local_coord' for spawn '" + spawn.name + "' at location=" + std::to_string(spawn.location.value)};
                }
                {
                    const auto& lc = spawnJson.at("local_coord");
                    if (!lc.is_array() || lc.size() != 2) {
                        return {false, "Invalid 'local_coord' format for spawn '" + spawn.name + "' at location=" + std::to_string(spawn.location.value)};
                    }
                    spawn.local_coord = std::make_pair(lc[0].get<int>(), lc[1].get<int>());
                }
            } catch (const nlohmann::json::exception& ex) {
                return {false, std::string{"Invalid spawn entry: "} + ex.what()};
            }

            graph.spawns.push_back(spawn);
        }
    }

    registry.setGraph(std::move(graph));
    return {true, {}};
}

} // namespace

WorldLoadResult loadWorldFromJsonString(std::string_view jsonData, WorldRegistry& registry) {
    try {
        auto data = nlohmann::json::parse(jsonData);
        return loadGraphFromJson(data, registry);
    } catch (const nlohmann::json::exception& ex) {
        return {false, std::string{"Failed to parse JSON: "} + ex.what()};
    }
}

WorldLoadResult loadWorldFromFile(const std::filesystem::path& path, WorldRegistry& registry) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return {false, "Unable to open world file: " + path.string()};
    }

    try {
        nlohmann::json data;
        stream >> data;
        return loadGraphFromJson(data, registry);
    } catch (const nlohmann::json::exception& ex) {
        return {false, std::string{"Failed to parse JSON: "} + ex.what()};
    }
}

} // namespace genesis::world
