#include "genesis/runtime/Runtime.hpp"

namespace genesis::runtime {

Runtime::Runtime(RuntimeConfig config)
    : m_config(std::move(config))
    , m_engine() {
    if (m_config.bootstrapSteps > 0) {
        m_engine.step(m_config.bootstrapSteps);
    }
}

void Runtime::step(std::uint64_t steps) {
    m_engine.step(steps);
}

void Runtime::run(std::uint64_t steps) {
    m_engine.run(steps);
}

const genesis::telemetry::TickTelemetry* Runtime::latestSnapshot() const noexcept {
    return m_engine.latestTelemetry();
}

std::unique_ptr<Runtime> createRuntime(RuntimeConfig config) {
    return std::make_unique<Runtime>(std::move(config));
}

} // namespace genesis::runtime
