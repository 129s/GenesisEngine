#include "genesis/runtime/Runtime.hpp"

#include <chrono>
#include <utility>

namespace genesis::runtime {

Runtime::Runtime(RuntimeConfig config)
    : m_config(std::move(config))
    , m_engine() {
    m_engine.setSnapshotCallback([this](const telemetry::TickTelemetry& tick) {
        SimulationSnapshot snapshot{};
        snapshot.version = m_snapshotVersion.fetch_add(1, std::memory_order_relaxed) + 1;
        snapshot.capturedAt = std::chrono::steady_clock::now();
        snapshot.telemetry = tick;
        m_snapshotBuffer.write(std::move(snapshot));
    });

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

const SimulationSnapshot* Runtime::latestSnapshot() const noexcept {
    return m_snapshotBuffer.latest();
}

std::unique_ptr<Runtime> createRuntime(RuntimeConfig config) {
    return std::make_unique<Runtime>(std::move(config));
}

} // namespace genesis::runtime
