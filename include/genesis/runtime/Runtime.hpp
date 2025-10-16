#pragma once

#include <cstdint>
#include <memory>

#include "genesis/core/Engine.hpp"
#include "genesis/telemetry/TelemetryBuffer.hpp"

namespace genesis::runtime {

struct RuntimeConfig {
    std::uint64_t bootstrapSteps{0};
};

class Runtime {
public:
    explicit Runtime(RuntimeConfig config = {});
    ~Runtime() = default;

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&) = delete;
    Runtime& operator=(Runtime&&) = delete;

    void step(std::uint64_t steps = 1);
    void run(std::uint64_t steps);

    [[nodiscard]] const genesis::telemetry::TickTelemetry* latestSnapshot() const noexcept;

    [[nodiscard]] genesis::core::Engine& engine() noexcept { return m_engine; }
    [[nodiscard]] const genesis::core::Engine& engine() const noexcept { return m_engine; }

private:
    RuntimeConfig m_config;
    genesis::core::Engine m_engine;
};

std::unique_ptr<Runtime> createRuntime(RuntimeConfig config = {});

} // namespace genesis::runtime

