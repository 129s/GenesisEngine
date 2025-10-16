#include "genesis/core/Engine.hpp"

#include <spdlog/spdlog.h>

namespace genesis::core {

Engine::Engine()
    : m_clock(SimulationClock::duration{500}) {
    spdlog::info("GenesisEngine core initialized");
}

void Engine::run(std::uint64_t maxSteps) {
    const auto delta = m_clock.stepDuration();
    std::uint64_t processed = 0;

    while (processed < maxSteps) {
        m_clock.advance(delta);

        if (!m_clock.stepReady()) {
            continue;
        }

        const auto current = m_clock.consumeStep();
        processStep(current);
        ++processed;
    }
}

void Engine::processStep(std::uint64_t stepIndex) {
    spdlog::debug("Processing simulation step {}", stepIndex);
    eventBus().updateAll();
}

} // namespace genesis::core
