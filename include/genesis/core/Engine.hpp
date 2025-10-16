#pragma once

#include <cstdint>

#include "genesis/core/SimulationClock.hpp"
#include "genesis/messaging/EventBus.hpp"
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

    SimulationClock m_clock;
    messaging::EventBus m_eventBus;
    world::WorldRegistry m_world;
};

} // namespace genesis::core
