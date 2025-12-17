#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <unordered_map>
#include <string>
#include <utility>
#include <vector>

#include "genesis/world/Namespace.hpp"
#include "genesis/world/WorldTypes.hpp"

namespace genesis::world {

// 基础 ID 类型（后续可替换为强类型 ID）
using MapId = std::uint32_t;
using SceneId = std::uint32_t;
using InteractionId = std::uint32_t;

enum class InteractionKind : std::uint8_t {
    Resource,
    Portal,
    Landmark,
    Unknown
};

struct Map {
    MapId id{0};
    std::string name;
    std::optional<nlohmann::json> meta;
};

struct MapEdge {
    MapId from{0};
    MapId to{0};
    bool bidirectional{true};
    double cost{1.0};
    std::optional<nlohmann::json> rules;
};

struct Scene {
    SceneId id{0};
    MapId mapId{0};
    std::optional<SceneId> parent;
    std::string name;
    std::optional<std::pair<int, int>> origin;
    std::optional<nlohmann::json> transform;
    std::optional<nlohmann::json> meta;
};

struct Interaction {
    InteractionId id{0};
    MapId mapId{0};
    SceneId sceneId{0};
    InteractionKind kind{InteractionKind::Unknown};
    std::pair<int, int> coordLocal{0, 0};
    std::optional<std::pair<int, int>> coordGlobal;
    std::string name;
    std::optional<nlohmann::json> meta;
    // Resource-specific (optional)
    std::optional<ResourceType> resourceType;
    std::optional<std::uint32_t> capacity;      // max units
    std::optional<std::uint32_t> regenPerStep;  // units per step

    [[nodiscard]] std::pair<int, int> worldCoord() const {
        return coordGlobal.value_or(coordLocal);
    }
};

struct Portal {
    MapId mapId{0}; // owning map (source)
    InteractionId interactionId{0};
    std::optional<std::string> channelId;
    bool oneWay{false};
    double teleportCost{0.0};
    MapId targetMapId{0};
    std::optional<SceneId> targetSceneId{};
    std::optional<std::pair<int, int>> targetCoord{};
};

struct TilemapMeta {
    int width{0};
    int height{0};
    int tileW{1};
    int tileH{1};
    std::optional<nlohmann::json> meta;
};

// 只读世界数据总线（替代旧 WorldRegistry 的查询职能）
class WorldDatabase {
public:
    virtual ~WorldDatabase() = default;

    // 顶层实体集合
    virtual const std::vector<Map>& maps() const = 0;
    virtual const std::vector<MapEdge>& mapEdges() const = 0;

    // 基于 Map 的细分查询
    virtual std::vector<Scene> scenes(MapId mapId) const = 0;
    virtual std::vector<Interaction> interactions(MapId mapId) const = 0;
    virtual std::vector<Portal> portals(MapId mapId) const = 0;
    virtual std::optional<TilemapMeta> tilemap(MapId mapId) const = 0;

    // 便捷查询
    virtual std::vector<MapEdge> mapEdgesFrom(MapId from) const = 0;
    virtual std::optional<Interaction> findInteraction(InteractionId id) const = 0;
};

// 简单的内存实现，供 Loader 与原型阶段使用
class InMemoryWorldDatabase final : public WorldDatabase {
public:
    // 构建期方法
    void clear();
    void addMap(Map m);
    void addMapEdge(MapEdge e);
    void addScene(Scene s);
    void addInteraction(Interaction i);
    void addPortal(Portal p);
    void setTilemap(MapId mapId, TilemapMeta tilemap);

    // WorldDatabase 实现
    const std::vector<Map>& maps() const override { return maps_; }
    const std::vector<MapEdge>& mapEdges() const override { return edges_; }
    std::vector<Scene> scenes(MapId mapId) const override;
    std::vector<Interaction> interactions(MapId mapId) const override;
    std::vector<Portal> portals(MapId mapId) const override;
    std::optional<TilemapMeta> tilemap(MapId mapId) const override;
    std::vector<MapEdge> mapEdgesFrom(MapId from) const override;
    std::optional<Interaction> findInteraction(InteractionId id) const override;

private:
    std::vector<Map> maps_;
    std::vector<MapEdge> edges_;
    std::vector<Scene> scenes_;
    std::vector<Interaction> interactions_;
    std::vector<Portal> portals_;
    std::unordered_map<MapId, TilemapMeta> tilemaps_;
};

} // namespace genesis::world
