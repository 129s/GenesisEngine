#pragma once

#include <cstdint>

#include "genesis/core/SimulationClock.hpp"
#include "genesis/messaging/EventBus.hpp"

namespace genesis::core {

class Engine {
public:
    Engine();

    void run(std::uint64_t maxSteps);

    [[nodiscard]] SimulationClock& clock() noexcept { return m_clock; }
    [[nodiscard]] const SimulationClock& clock() const noexcept { return m_clock; }

    [[nodiscard]] messaging::EventBus& eventBus() noexcept { return m_eventBus; }
    [[nodiscard]] const messaging::EventBus& eventBus() const noexcept { return m_eventBus; }

private:
    void processStep(std::uint64_t stepIndex);

    SimulationClock m_clock;
    messaging::EventBus m_eventBus;
};

} // namespace genesis::core
