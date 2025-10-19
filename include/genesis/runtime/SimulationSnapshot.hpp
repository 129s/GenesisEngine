#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <utility>

#include "genesis/telemetry/TelemetryBuffer.hpp"

namespace genesis::runtime {

struct SimulationSnapshot {
    std::uint64_t version{0};
    std::chrono::steady_clock::time_point capturedAt{};
    telemetry::TickTelemetry telemetry;
};

class SimulationSnapshotBuffer {
public:
    SimulationSnapshotBuffer()
        : m_readIndex(0)
        , m_writeIndex(0)
        , m_hasSnapshot(false) {
    }

    void write(SimulationSnapshot snapshot) {
        const std::uint32_t nextIndex = (m_writeIndex + 1) % m_buffers.size();
        m_buffers[nextIndex] = std::move(snapshot);
        m_writeIndex = nextIndex;
        m_readIndex.store(nextIndex, std::memory_order_release);
        m_hasSnapshot.store(true, std::memory_order_release);
    }

    [[nodiscard]] const SimulationSnapshot* latest() const noexcept {
        if (!m_hasSnapshot.load(std::memory_order_acquire)) {
            return nullptr;
        }
        const auto index = m_readIndex.load(std::memory_order_acquire);
        return &m_buffers[index];
    }

    void clear() noexcept {
        m_hasSnapshot.store(false, std::memory_order_release);
    }

private:
    std::array<SimulationSnapshot, 2> m_buffers;
    std::atomic<std::uint32_t> m_readIndex;
    std::uint32_t m_writeIndex;
    std::atomic<bool> m_hasSnapshot;
};

} // namespace genesis::runtime
