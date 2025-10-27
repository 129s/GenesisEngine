#include "sandbox/gui/RuntimeBridge.hpp"
#include "genesis/world/WorldDatabaseLoader.hpp"

#include <algorithm>
#include <optional>

namespace genesis::sandbox::gui
{
    namespace
{
constexpr float kHorizontalSpacing = 180.0f;
constexpr float kVerticalSpacing = 140.0f;
constexpr float kMinExtent = 100.0f;

RuntimeBridge::Vector2 computeExtent(std::size_t maxPerLevel, std::size_t levelCount)
{
    const float width = std::max<std::size_t>(1, maxPerLevel) * kHorizontalSpacing;
    const float height = std::max<std::size_t>(1, levelCount) * kVerticalSpacing;
    return RuntimeBridge::Vector2{std::max(width, kMinExtent), std::max(height, kMinExtent)};
}

// 注：buildWorldAtlas(WorldDatabase) 与 loadWorldDatabaseFolder 的定义移至匿名命名空间外部

} // namespace
RuntimeBridge::WorldAtlas RuntimeBridge::buildWorldAtlas(const genesis::world::WorldDatabase& db)
{
    WorldAtlas atlas;

    const auto& maps = db.maps();
    std::size_t levelCount = 1;
    std::size_t maxPerLevel = maps.size();
    atlas.extent = computeExtent(maxPerLevel, levelCount);

    float x = 0.0f;
    for (const auto& m : maps)
    {
        WorldAtlas::Node n{};
        n.id = m.id;
        n.parent = std::nullopt;
        n.name = m.name;
        n.position = Vector2{x, 0.0f};
        atlas.nodeLookup.emplace(n.id, n.position);
        atlas.nodes.push_back(std::move(n));
        x += kHorizontalSpacing;
    }

    for (const auto& e : db.mapEdges())
    {
        WorldAtlas::Edge ae{};
        ae.from = e.from;
        ae.to = e.to;
        ae.bidirectional = e.bidirectional;
        atlas.edges.push_back(std::move(ae));
    }

    for (const auto& m : maps)
    {
        for (const auto& inter : db.interactions(m.id))
        {
            if (inter.kind == genesis::world::InteractionKind::Resource)
            {
                Vector2 position{0.0f, 0.0f};
                if (auto p = atlas.nodePosition(m.id)) position = *p;
                WorldAtlas::Spawn s{};
                s.name = inter.name;
                s.type = genesis::world::ResourceType::Food; // 显示用途：细化类型映射可在数据扩展时加入
                s.mapId = m.id;
                s.interactionId = inter.id;
                s.position = position;
                atlas.spawns.push_back(std::move(s));
            }
        }
    }

    return atlas;
}

void RuntimeBridge::rebuildAtlasOnRuntimeThread()
{
    WorldAtlas nextAtlas{};
    if (auto db = runtime_.worldDatabase()) {
        nextAtlas = buildWorldAtlas(*db);
    } else if (worldDb_) {
        nextAtlas = buildWorldAtlas(*worldDb_);
    } else {
        nextAtlas = {};
    }
    std::lock_guard snapLock(snapshotMutex_);
    atlas_ = std::move(nextAtlas);
    snapshots_.clear();
}
} // namespace genesis::sandbox::gui
