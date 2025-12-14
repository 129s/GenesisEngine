#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/entt.hpp>

#include "genesis/messaging/EventBus.hpp"
#include "genesis/world/WorldDatabase.hpp"
#include "genesis/world/components/ResourceInventory.hpp"
#include "genesis/world/components/ResourceSpawn.hpp"

namespace genesis::world::system {

class ResourceSystem {
public:
    ResourceSystem(WorldDatabase& database, genesis::messaging::EventBus& eventBus);

    void initialize(entt::registry& registry);
    void reset(entt::registry& registry);
    void tick(entt::registry& registry, std::uint64_t stepIndex);

    std::uint32_t consume(entt::registry& registry, genesis::world::ResourceType type, std::uint32_t amount, genesis::world::InteractionId preferred);
    std::uint32_t consumeFromInteraction(entt::registry& registry,
                                         genesis::world::InteractionId interaction,
                                         genesis::world::ResourceType type,
                                         std::uint32_t amount);

    std::uint32_t produceAtInteraction(entt::registry& registry, genesis::world::InteractionId interaction, std::uint32_t amount);

    struct SpawnState {
        genesis::world::InteractionId interaction{0};
        genesis::world::MapId mapId{0};
        std::string name;
        genesis::world::ResourceType type{genesis::world::ResourceType::Food};
        std::uint32_t current{0};
        std::uint32_t capacity{0};
    };

    [[nodiscard]] std::optional<SpawnState> spawnState(const entt::registry& registry, genesis::world::InteractionId interaction) const;

    struct TelemetryDelta {
        std::uint32_t consumed{0};
        std::uint32_t produced{0};
    };

    [[nodiscard]] TelemetryDelta telemetryDelta(genesis::world::InteractionId interaction) const noexcept;
    void clearTelemetryDeltas() noexcept;

    template <typename Fn>
    void forEachSpawn(const entt::registry& registry, Fn&& fn) const {
        if (!m_initialized) {
            return;
        }
        for (const auto entity : m_spawnEntities) {
            if (!registry.valid(entity)) {
                continue;
            }
            const auto& inventory = registry.get<components::ResourceInventory>(entity);
            const auto& spawn = registry.get<components::ResourceSpawn>(entity);
            fn(spawn, inventory);
        }
    }

private:

    WorldDatabase& m_db;
    genesis::messaging::EventBus& m_eventBus;
    std::vector<entt::entity> m_spawnEntities;
    std::unordered_map<genesis::world::InteractionId, entt::entity> m_spawnByInteraction;
    std::unordered_map<genesis::world::InteractionId, TelemetryDelta> m_telemetryDeltas;
    bool m_initialized{false};
};

} // namespace genesis::world::system

