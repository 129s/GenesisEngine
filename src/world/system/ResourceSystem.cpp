#include "genesis/world/system/ResourceSystem.hpp"

#include <algorithm>

#include <spdlog/spdlog.h>

#include "genesis/world/components/ResourceInventory.hpp"
#include "genesis/world/components/ResourceSpawn.hpp"

namespace genesis::world::system {

ResourceSystem::ResourceSystem(WorldRegistry& registry)
    : m_world(registry) {
}

void ResourceSystem::initialize(entt::registry& registry) {
    if (m_initialized) {
        return;
    }

    const auto& spawns = m_world.resourceSpawns();
    m_spawnEntities.reserve(spawns.size());

    for (const auto& spawn : spawns) {
        const auto entity = registry.create();
        auto& inventory = registry.emplace<components::ResourceInventory>(entity);
        inventory.capacity = spawn.capacity;
        inventory.current = spawn.capacity;

        auto& info = registry.emplace<components::ResourceSpawn>(entity);
        info.name = spawn.name;
        info.type = spawn.type;
        info.location = spawn.location;
        info.ratePerStep = spawn.ratePerStep;

        m_spawnEntities.push_back(entity);
    }

    spdlog::info("ResourceSystem initialized {} spawns", m_spawnEntities.size());
    m_initialized = true;
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

} // namespace genesis::world::system

