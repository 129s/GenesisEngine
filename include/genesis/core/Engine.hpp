#pragma once

#include <cstdint>

#include <entt/entt.hpp>

#include "genesis/core/SimulationClock.hpp"
#include "genesis/messaging/EventBus.hpp"
#include "genesis/agents/NeedSystem.hpp"
#include "genesis/agents/NeedSatisfier.hpp"
#include "genesis/world/WorldRegistry.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

namespace genesis::core {

class Engine {
public:
    struct ResourceRequestResult {
        std::uint32_t fulfilled{0};
        std::uint32_t requested{0};
    };

    Engine();

    void run(std::uint64_t maxSteps);

    [[nodiscard]] SimulationClock& clock() noexcept { return m_clock; }
    [[nodiscard]] const SimulationClock& clock() const noexcept { return m_clock; }

    [[nodiscard]] messaging::EventBus& eventBus() noexcept { return m_eventBus; }
    [[nodiscard]] const messaging::EventBus& eventBus() const noexcept { return m_eventBus; }

    [[nodiscard]] world::WorldRegistry& world() noexcept { return m_world; }
    [[nodiscard]] const world::WorldRegistry& world() const noexcept { return m_world; }

    ResourceRequestResult requestResource(world::ResourceType type, std::uint32_t amount, world::LocationId preferredLocation);

private:
    void processStep(std::uint64_t stepIndex);
    void loadInitialWorld();
    void configureNeedDefaults();
    void spawnDemoAgents();

    SimulationClock m_clock;
    messaging::EventBus m_eventBus;
    world::WorldRegistry m_world;
    entt::registry m_registry;
    agents::NeedSystem m_needSystem;
    agents::NeedSatisfier m_needSatisfier;
    world::system::ResourceSystem m_resourceSystem;
};

} // namespace genesis::core
