#include "genesis/base/SimulationClock.hpp"

#include <algorithm>

namespace genesis::base {

SimulationClock::SimulationClock(duration step, duration maxCatchUp)
    : m_step(step)
    , m_maxCatchUp(maxCatchUp) {}

void SimulationClock::advance(duration delta) {
    m_accumulator += delta;
    const auto maxAccumulated = std::max(m_step, m_maxCatchUp);
    m_accumulator = std::min(m_accumulator, maxAccumulated);
}

bool SimulationClock::stepReady() const {
    return m_accumulator >= m_step;
}

std::uint64_t SimulationClock::consumeStep() {
    if (!stepReady()) {
        return m_currentStep;
    }
    m_accumulator -= m_step;
    return ++m_currentStep;
}

void SimulationClock::setMaxCatchUp(duration value) noexcept {
    m_maxCatchUp = std::max(value, m_step);
}

} // namespace genesis::base

