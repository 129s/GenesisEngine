#include "genesis/world/generation/NoiseGridGenerator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

#include <nlohmann/json.hpp>

using genesis::world::LocationKind;
using genesis::world::LocationNode;
using genesis::world::LocationId;
using genesis::world::PathEdge;
using genesis::world::ResourceSpawn;
using genesis::world::ResourceType;
using genesis::world::InvalidLocation;

namespace genesis::world::generation {

namespace {

double normalizedNoise(std::uint64_t seed, std::uint32_t x, std::uint32_t y, std::uint64_t salt = 0) {
    std::uint64_t value = seed ^ salt;
    value += static_cast<std::uint64_t>(x) * 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value += static_cast<std::uint64_t>(y) * 0x94D049BB133111EBULL;
    value ^= (value >> 31U);
    return static_cast<double>(value) / static_cast<double>(std::numeric_limits<std::uint64_t>::max());
}

std::string toString(LocationKind kind) {
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

nlohmann::json toJson(const LocationNode& node) {
    nlohmann::json jsonNode;
    jsonNode["id"] = node.id.value;
    if (node.parent != InvalidLocation) {
        jsonNode["parent"] = node.parent.value;
    }
    if (!node.name.empty()) {
        jsonNode["name"] = node.name;
    }
    jsonNode["kind"] = toString(node.kind);
    jsonNode["navigable"] = node.navigable;
    if (!node.terrain.empty()) {
        jsonNode["terrain"] = node.terrain;
    }
    return jsonNode;
}

nlohmann::json toJson(const PathEdge& edge) {
    nlohmann::json jsonEdge;
    jsonEdge["from"] = edge.from.value;
    jsonEdge["to"] = edge.to.value;
    jsonEdge["cost"] = edge.cost;
    jsonEdge["bidirectional"] = edge.bidirectional;
    return jsonEdge;
}

nlohmann::json toJson(const ResourceSpawn& spawn) {
    nlohmann::json jsonSpawn;
    if (!spawn.name.empty()) {
        jsonSpawn["name"] = spawn.name;
    }
    jsonSpawn["type"] = "Food"; // only Food supported for now
    jsonSpawn["location"] = spawn.location.value;
    jsonSpawn["capacity"] = spawn.capacity;
    jsonSpawn["rate_per_step"] = spawn.ratePerStep;
    return jsonSpawn;
}

} // namespace

NoiseGenerationResult NoiseGridGenerator::generate(std::uint64_t seed, const NoiseGridConfig& config) const {
    if (config.width == 0 || config.height == 0) {
        throw std::invalid_argument("NoiseGridGenerator requires width and height greater than zero");
    }

    NoiseGenerationResult result;
    auto& graph = result.graph;

    const LocationId rootId{1};
    LocationNode root{};
    root.id = rootId;
    root.parent = InvalidLocation;
    root.name = "NoiseMap";
    root.kind = LocationKind::Region;
    root.navigable = true;
    root.terrain = "Region";
    graph.nodes.push_back(root);

    struct CellInfo {
        LocationId id;
        TerrainType terrain{TerrainType::Stone};
        bool navigable{false};
        std::uint32_t x{0};
        std::uint32_t y{0};
        double noiseValue{0.0};
    };

    std::vector<CellInfo> cells;
    cells.reserve(static_cast<std::size_t>(config.width) * config.height);

    std::uint32_t nextId = 2;
    const double spawnDensity = std::clamp(config.soilSpawnDensity, 0.0, 1.0);

    CellInfo* bestSpawnCandidate = nullptr;
    double bestSpawnScore = -1.0;

    for (std::uint32_t y = 0; y < config.height; ++y) {
        for (std::uint32_t x = 0; x < config.width; ++x) {
            CellInfo cell{};
            cell.id = LocationId{nextId++};
            cell.x = x;
            cell.y = y;
            cell.noiseValue = normalizedNoise(seed, x, y);
            const bool soil = cell.noiseValue >= config.threshold;
            cell.terrain = soil ? TerrainType::Soil : TerrainType::Stone;
            cell.navigable = soil;

            LocationNode node{};
            node.id = cell.id;
            node.parent = rootId;
            node.name = soil ? "Soil" : "Stone";
            node.kind = LocationKind::Point;
            node.navigable = soil;
            node.terrain = soil ? "Soil" : "Stone";
            graph.nodes.push_back(std::move(node));

            cells.push_back(cell);

            if (soil) {
                const double spawnScore = normalizedNoise(seed, x, y, 0xA5B35705893B9ADFULL);
                if (spawnScore > bestSpawnScore) {
                    bestSpawnScore = spawnScore;
                    bestSpawnCandidate = &cells.back();
                }
                if (spawnScore <= spawnDensity) {
                    ResourceSpawn spawn{};
                    spawn.name = "Soil Food (" + std::to_string(x) + "," + std::to_string(y) + ")";
                    spawn.type = ResourceType::Food;
                    spawn.location = cell.id;
                    spawn.capacity = config.resourceCapacity;
                    spawn.ratePerStep = config.resourceRatePerStep;
                    graph.spawns.push_back(std::move(spawn));
                }
            }
        }
    }

    const auto indexOf = [width = config.width](std::uint32_t x, std::uint32_t y) -> std::size_t {
        return static_cast<std::size_t>(y) * width + x;
    };

    for (std::uint32_t y = 0; y < config.height; ++y) {
        for (std::uint32_t x = 0; x < config.width; ++x) {
            const CellInfo& cell = cells[indexOf(x, y)];
            if (!cell.navigable) {
                continue;
            }

            const std::array<std::pair<int, int>, 4> directions{{
                {0, -1},
                {1, 0},
                {0, 1},
                {-1, 0},
            }};

            for (const auto [dx, dy] : directions) {
                const int nx = static_cast<int>(x) + dx;
                const int ny = static_cast<int>(y) + dy;
                if (nx < 0 || ny < 0) {
                    continue;
                }
                if (nx >= static_cast<int>(config.width) || ny >= static_cast<int>(config.height)) {
                    continue;
                }

                const CellInfo& neighbor = cells[indexOf(static_cast<std::uint32_t>(nx), static_cast<std::uint32_t>(ny))];
                if (!neighbor.navigable) {
                    continue;
                }

                if (neighbor.id.value <= cell.id.value) {
                    continue; // avoid duplicate bidirectional edges
                }

                PathEdge edge{};
                edge.from = cell.id;
                edge.to = neighbor.id;
                edge.cost = 1.0f;
                edge.bidirectional = true;
                graph.edges.push_back(edge);
            }
        }
    }

    if (graph.spawns.empty() && bestSpawnCandidate != nullptr) {
        ResourceSpawn fallback{};
        fallback.name = "Soil Food (" + std::to_string(bestSpawnCandidate->x) + "," +
                        std::to_string(bestSpawnCandidate->y) + ")";
        fallback.type = ResourceType::Food;
        fallback.location = bestSpawnCandidate->id;
        fallback.capacity = config.resourceCapacity;
        fallback.ratePerStep = config.resourceRatePerStep;
        graph.spawns.push_back(std::move(fallback));
    }

    result.layout.width = config.width * 2 + 1;
    result.layout.height = config.height + 1;
    result.layout.nodes.reserve(cells.size());

    for (const auto& cell : cells) {
        LayoutNode entry{};
        entry.id = cell.id;
        entry.label = (cell.terrain == TerrainType::Soil) ? "Soil" : "Stone";
        entry.x = static_cast<std::int32_t>(cell.x * 2 + 1);
        entry.y = static_cast<std::int32_t>(cell.y + 1);
        result.layout.nodes.push_back(std::move(entry));
    }

    return result;
}

void writeNoiseGenerationOutputs(const NoiseGenerationResult& result,
                                 const std::filesystem::path& worldPath,
                                 const std::filesystem::path& layoutPath) {
    nlohmann::json worldJson;
    worldJson["locations"] = nlohmann::json::array();
    worldJson["edges"] = nlohmann::json::array();
    worldJson["spawns"] = nlohmann::json::array();

    for (const auto& node : result.graph.nodes) {
        worldJson["locations"].push_back(toJson(node));
    }
    for (const auto& edge : result.graph.edges) {
        worldJson["edges"].push_back(toJson(edge));
    }
    for (const auto& spawn : result.graph.spawns) {
        worldJson["spawns"].push_back(toJson(spawn));
    }

    std::filesystem::create_directories(worldPath.parent_path());
    std::ofstream worldStream(worldPath);
    if (!worldStream) {
        throw std::runtime_error("Failed to open world output file: " + worldPath.string());
    }
    worldStream << worldJson.dump(2);

    nlohmann::json layoutJson;
    layoutJson["width"] = result.layout.width;
    layoutJson["height"] = result.layout.height;
    layoutJson["nodes"] = nlohmann::json::array();
    for (const auto& node : result.layout.nodes) {
        nlohmann::json layoutNode;
        layoutNode["id"] = node.id.value;
        layoutNode["label"] = node.label;
        layoutNode["x"] = node.x;
        layoutNode["y"] = node.y;
        layoutJson["nodes"].push_back(std::move(layoutNode));
    }

    std::filesystem::create_directories(layoutPath.parent_path());
    std::ofstream layoutStream(layoutPath);
    if (!layoutStream) {
        throw std::runtime_error("Failed to open layout output file: " + layoutPath.string());
    }
    layoutStream << layoutJson.dump(2);
}

} // namespace genesis::world::generation
