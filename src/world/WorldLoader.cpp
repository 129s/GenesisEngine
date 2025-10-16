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

            bool kindOk = true;
            node.kind = parseLocationKind(nodeJson.value("kind", std::string{"Point"}), kindOk);
            if (!kindOk) {
                return {false, "Unknown location kind: " + nodeJson.value("kind", std::string{})};
            }
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
