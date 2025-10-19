#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "genesis/runtime/RuntimeEvents.hpp"
#include "genesis/telemetry/TelemetryBuffer.hpp"

namespace genesis::runtime {

struct SimulationSnapshot {
    std::uint64_t version{0};
    std::chrono::steady_clock::time_point capturedAt{};
    telemetry::TickTelemetry telemetry;
    std::vector<RuntimeEventReport> events;
};

class SimulationSnapshotBuffer {
public:
    struct SnapshotView {
        const SimulationSnapshot* latest{nullptr};
        const SimulationSnapshot* previous{nullptr};
    };

    SimulationSnapshotBuffer()
        : m_readIndex(0)
        , m_writeIndex(0)
        , m_hasSnapshot(false)
        , m_writeCount(0) {
    }

    void write(SimulationSnapshot snapshot) {
        const std::uint32_t nextIndex = (m_writeIndex + 1) % m_buffers.size();
        m_buffers[nextIndex] = std::move(snapshot);
        m_writeIndex = nextIndex;
        m_readIndex.store(nextIndex, std::memory_order_release);
        m_hasSnapshot.store(true, std::memory_order_release);
        m_writeCount.fetch_add(1, std::memory_order_release);
    }

    [[nodiscard]] const SimulationSnapshot* latest() const noexcept {
        if (!m_hasSnapshot.load(std::memory_order_acquire)) {
            return nullptr;
        }
        const auto index = m_readIndex.load(std::memory_order_acquire);
        return &m_buffers[index];
    }

    [[nodiscard]] std::optional<SnapshotView> latestPair() const noexcept {
        if (!m_hasSnapshot.load(std::memory_order_acquire)) {
            return std::nullopt;
        }

        SnapshotView view{};
        const auto currentIndex = m_readIndex.load(std::memory_order_acquire);
        view.latest = &m_buffers[currentIndex];

        const auto writes = m_writeCount.load(std::memory_order_acquire);
        if (writes > 1) {
            const auto previousIndex = (currentIndex + m_buffers.size() - 1) % m_buffers.size();
            view.previous = &m_buffers[previousIndex];
        }

        return view;
    }

    void clear() noexcept {
        m_hasSnapshot.store(false, std::memory_order_release);
        m_writeCount.store(0, std::memory_order_release);
    }

private:
    std::array<SimulationSnapshot, 2> m_buffers;
    std::atomic<std::uint32_t> m_readIndex;
    std::uint32_t m_writeIndex;
    std::atomic<bool> m_hasSnapshot;
    std::atomic<std::uint64_t> m_writeCount;
};

} // namespace genesis::runtime
