#pragma once

#include <chrono>
#include <cstdint>

namespace genesis::core {

class SimulationClock {
public:
    using clock = std::chrono::steady_clock;
    using duration = std::chrono::milliseconds;

    explicit SimulationClock(duration step = duration{500}, duration maxCatchUp = duration{5000});

    void advance(duration delta);

    [[nodiscard]] bool stepReady() const;

    std::uint64_t consumeStep();

    [[nodiscard]] duration stepDuration() const noexcept { return m_step; }
    [[nodiscard]] std::uint64_t currentStep() const noexcept { return m_currentStep; }
    [[nodiscard]] duration accumulated() const noexcept { return m_accumulator; }
    [[nodiscard]] duration maxCatchUp() const noexcept { return m_maxCatchUp; }

    void setMaxCatchUp(duration value) noexcept;

private:
    duration m_step;
    duration m_accumulator{};
    duration m_maxCatchUp;
    std::uint64_t m_currentStep{0};
};

} // namespace genesis::core
