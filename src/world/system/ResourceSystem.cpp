#include "genesis/world/system/ResourceSystem.hpp"

#include <algorithm>
#include <utility>

#include <spdlog/spdlog.h>

#include "genesis/messaging/EventBus.hpp"
#include "genesis/world/components/ResourceInventory.hpp"
#include "genesis/world/components/ResourceSpawn.hpp"
#include "genesis/world/events/ResourceEvents.hpp"

namespace genesis::world::system {

ResourceSystem::ResourceSystem(WorldDatabase& database, genesis::messaging::EventBus& eventBus)
    : m_db(database)
    , m_eventBus(eventBus) {
}

void ResourceSystem::initialize(entt::registry& registry) {
    if (m_initialized) {
        return;
    }

    // Build spawn entities from database interactions of kind Resource
    std::size_t total = 0;
    for (const auto& m : m_db.maps()) {
        const auto interactions = m_db.interactions(m.id);
        for (const auto& it : interactions) {
            if (it.kind != InteractionKind::Resource) continue;

            const auto entity = registry.create();
            auto& inventory = registry.emplace<components::ResourceInventory>(entity);
            inventory.capacity = it.capacity.value_or(0);
            inventory.current = inventory.capacity;

            auto& info = registry.emplace<components::ResourceSpawn>(entity);
            info.name = it.name;
            info.type = it.resourceType.value_or(ResourceType::Food);
            info.interaction = it.id;
            info.mapId = it.mapId;
            info.ratePerStep = it.regenPerStep.value_or(0);

            m_spawnEntities.push_back(entity);
            ++total;
        }
    }

    spdlog::info("ResourceSystem initialized {} spawns", total);
    m_initialized = true;
}

void ResourceSystem::reset(entt::registry& registry) {
    for (const auto entity : m_spawnEntities) {
        if (registry.valid(entity)) {
            registry.destroy(entity);
        }
    }
    m_spawnEntities.clear();
    m_initialized = false;
}

void ResourceSystem::tick(entt::registry& registry, std::uint64_t stepIndex) {
    if (!m_initialized) {
        initialize(registry);
    }

    for (const auto entity : m_spawnEntities) {
        auto& inventory = registry.get<components::ResourceInventory>(entity);
        const auto& info = registry.get<components::ResourceSpawn>(entity);

        if (inventory.current >= inventory.capacity || info.ratePerStep == 0U) {
            continue;
        }

        const std::uint32_t space = inventory.capacity - inventory.current;
        const std::uint32_t produced = std::min(info.ratePerStep, space);

        if (produced > 0U) {
            inventory.current += produced;
            spdlog::debug("ResourceSystem step {}: {} produced {} -> {}/{}",
                stepIndex, info.name, produced, inventory.current, inventory.capacity);
        }
    }
}

std::uint32_t ResourceSystem::consume(entt::registry& registry,
                                      genesis::world::ResourceType type,
                                      std::uint32_t amount,
                                      genesis::world::InteractionId preferred) {
    if (!m_initialized) {
        initialize(registry);
    }

    std::uint32_t remaining = amount;

    auto consumeFromEntity = [&](entt::entity entity, const components::ResourceSpawn& spawn) {
        auto& inventory = registry.get<components::ResourceInventory>(entity);
        if (inventory.current == 0U || remaining == 0U) {
            return;
        }

        const auto taken = std::min(inventory.current, remaining);
        inventory.current -= taken;
        remaining -= taken;

        genesis::world::events::ResourceConsumed event{};
        event.name = spawn.name;
        event.type = spawn.type;
        event.interaction = spawn.interaction;
        event.mapId = spawn.mapId;
        event.amount = taken;
        event.remaining = inventory.current;
        m_eventBus.trigger<genesis::world::events::ResourceConsumed>(std::move(event));

        const auto threshold = static_cast<std::uint32_t>(inventory.capacity * 0.2f);
        if (inventory.current <= threshold) {
            genesis::world::events::ResourceLowStock low{};
            low.name = spawn.name;
            low.type = spawn.type;
            low.interaction = spawn.interaction;
            low.mapId = spawn.mapId;
            low.remaining = inventory.current;
            low.capacity = inventory.capacity;
            m_eventBus.trigger<genesis::world::events::ResourceLowStock>(std::move(low));
        }
    };

    if (preferred != 0) {
        for (const auto entity : m_spawnEntities) {
            const auto& spawn = registry.get<components::ResourceSpawn>(entity);
            if (spawn.type == type && spawn.interaction == preferred) {
                consumeFromEntity(entity, spawn);
            }
        }
    }

    if (remaining == 0U) {
        return amount;
    }

    for (const auto entity : m_spawnEntities) {
        const auto& spawn = registry.get<components::ResourceSpawn>(entity);
        if (spawn.type != type || spawn.interaction == preferred) {
            continue;
        }

        consumeFromEntity(entity, spawn);
        if (remaining == 0U) {
            break;
        }
    }

    return amount - remaining;
}

} // namespace genesis::world::system
