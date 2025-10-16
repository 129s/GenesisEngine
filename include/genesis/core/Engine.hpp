#pragma once

#include <cstdint>

#include <entt/entt.hpp>

#include "genesis/core/SimulationClock.hpp"
#include "genesis/messaging/EventBus.hpp"
#include "genesis/agents/NeedSystem.hpp"
#include "genesis/world/WorldRegistry.hpp"

namespace genesis::core {

class Engine {
public:
    Engine();

    void run(std::uint64_t maxSteps);

    [[nodiscard]] SimulationClock& clock() noexcept { return m_clock; }
    [[nodiscard]] const SimulationClock& clock() const noexcept { return m_clock; }

    [[nodiscard]] messaging::EventBus& eventBus() noexcept { return m_eventBus; }
    [[nodiscard]] const messaging::EventBus& eventBus() const noexcept { return m_eventBus; }

    [[nodiscard]] world::WorldRegistry& world() noexcept { return m_world; }
    [[nodiscard]] const world::WorldRegistry& world() const noexcept { return m_world; }

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
};

} // namespace genesis::core
