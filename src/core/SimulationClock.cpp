#include "genesis/core/SimulationClock.hpp"

#include <algorithm>

namespace genesis::core {

SimulationClock::SimulationClock(duration step, duration maxCatchUp)
    : m_step(step)
    , m_maxCatchUp(maxCatchUp) {
    if (m_step <= duration::zero()) {
        m_step = duration{1};
    }
    if (m_maxCatchUp < m_step) {
        m_maxCatchUp = m_step;
    }
}

void SimulationClock::advance(duration delta) {
    if (delta <= duration::zero()) {
        return;
    }

    const auto clamped = std::min(delta, m_maxCatchUp);
    m_accumulator += clamped;

    if (m_accumulator > m_maxCatchUp) {
        m_accumulator = m_maxCatchUp;
    }
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
    if (value < m_step) {
        value = m_step;
    }
    m_maxCatchUp = value;
    if (m_accumulator > m_maxCatchUp) {
        m_accumulator = m_maxCatchUp;
    }
}

} // namespace genesis::core
