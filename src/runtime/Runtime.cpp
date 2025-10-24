#include "genesis/runtime/Runtime.hpp"

#include <chrono>
#include <exception>
#include <filesystem>
#include <iterator>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <utility>
#include <vector>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "genesis/world/WorldDatabaseLoader.hpp"
#include "genesis/world/WorldLoader.hpp"

namespace genesis::runtime {

Runtime::Runtime(RuntimeConfig config)
    : m_config(std::move(config))
    , m_engine() {
    m_engine.setSnapshotCallback([this](const telemetry::TickTelemetry& tick) {
        SimulationSnapshot snapshot{};
        snapshot.version = m_snapshotVersion.fetch_add(1, std::memory_order_relaxed) + 1;
        snapshot.capturedAt = std::chrono::steady_clock::now();
        snapshot.telemetry = tick;
        {
            std::scoped_lock lock(m_eventMutex);
            if (!m_eventsSinceLastSnapshot.empty()) {
                snapshot.events = std::move(m_eventsSinceLastSnapshot);
                m_eventsSinceLastSnapshot.clear();
            }
        }
        m_snapshotBuffer.write(std::move(snapshot));
    });

    try {
        if (!spdlog::default_logger()) {
            auto logger = spdlog::stdout_color_mt("genesis");
            spdlog::set_default_logger(std::move(logger));
            spdlog::set_level(spdlog::level::info);
        }
    } catch (...) {
        // Logging initialization is best-effort in runtime context
    }

    if (m_config.initialWorldPath) {
        const auto absolute = std::filesystem::absolute(*m_config.initialWorldPath);
        const auto loadResult = loadWorldFromFile(absolute);
        if (!loadResult.success) {
            spdlog::error("Failed to load initial world {}: {}", absolute.string(), loadResult.error);
        }
    }

    if (m_config.bootstrapSteps > 0) {
        m_engine.step(m_config.bootstrapSteps);
    }
}

void Runtime::step(std::uint64_t steps) {
    for (std::uint64_t processed = 0; processed < steps; ++processed) {
        drainPendingEvents();
        m_engine.step(1);
    }
}

void Runtime::run(std::uint64_t steps) {
    for (std::uint64_t processed = 0; processed < steps; ++processed) {
        drainPendingEvents();
        m_engine.run(1);
    }
}

const SimulationSnapshot* Runtime::latestSnapshot() const noexcept {
    return m_snapshotBuffer.latest();
}

std::optional<SimulationSnapshotDiff> Runtime::latestSnapshotDiff() const noexcept {
    const auto view = m_snapshotBuffer.latestPair();
    if (!view) {
        return std::nullopt;
    }
    return diffSnapshots(view->previous, *view->latest);
}

std::uint64_t Runtime::enqueueEvent(RuntimeEvent event) {
    event.id = m_nextEventId.fetch_add(1, std::memory_order_relaxed);
    const auto id = event.id;
    event.enqueuedAt = std::chrono::steady_clock::now();
    std::scoped_lock lock(m_eventMutex);
    m_pendingEvents.push(std::move(event));
    return id;
}

Runtime::WorldGenerationResult Runtime::generateWorldFromConfig(const std::filesystem::path& configPath, std::optional<std::uint64_t>, std::optional<std::filesystem::path>) {
    WorldGenerationResult result{};
    result.configPath = std::filesystem::absolute(configPath);
    result.success = false;
    result.error = "world generation is not supported in v2 runtime";
    m_lastWorldGen = result;
    return *m_lastWorldGen;
}

genesis::world::WorldLoadResult Runtime::loadWorldFromFile(const std::filesystem::path& path) {
    const auto absolute = std::filesystem::absolute(path);
    auto result = m_engine.loadWorldFromFile(absolute);
    if (result.success) {
        spdlog::info("Runtime loaded world DB from {}", absolute.string());
    }
    return result;
}

genesis::world::WorldSaveResult Runtime::saveWorldToFile(const std::filesystem::path&) const {
    genesis::world::WorldSaveResult r{};
    r.success = false;
    r.error = "world save not implemented for v2";
    return r;
}

std::unique_ptr<Runtime> createRuntime(RuntimeConfig config) {
    return std::make_unique<Runtime>(std::move(config));
}

std::shared_ptr<genesis::world::WorldDatabase> Runtime::worldDatabase() const noexcept {
    return m_engine.worldDatabase();
}

void Runtime::drainPendingEvents() {
    std::queue<RuntimeEvent> local;
    {
        std::scoped_lock lock(m_eventMutex);
        if (m_pendingEvents.empty()) {
            return;
        }
        std::swap(local, m_pendingEvents);
    }

    std::vector<RuntimeEventReport> executed;
    executed.reserve(local.size());

    while (!local.empty()) {
        auto event = std::move(local.front());
        local.pop();

        RuntimeEventReport report{};
        report.id = event.id;
        report.kind = event.kind;
        report.label = std::move(event.label);
        report.payloadJson = std::move(event.payloadJson);
        report.enqueuedAt = event.enqueuedAt;
        report.executedAt = std::chrono::steady_clock::now();

        try {
            if (event.runtimeHandler) {
                event.runtimeHandler(*this);
            } else if (event.handler) {
                event.handler(m_engine);
            }
            report.success = true;
        } catch (const std::exception& ex) {
            report.success = false;
            report.message = ex.what();
        } catch (...) {
            report.success = false;
            report.message = "unknown error";
        }

        if (event.onComplete) {
            try {
                event.onComplete(report);
            } catch (const std::exception& ex) {
                spdlog::warn("RuntimeEvent onComplete failed: {}", ex.what());
            } catch (...) {
                spdlog::warn("RuntimeEvent onComplete failed with unknown error");
            }
        }

        executed.push_back(std::move(report));
    }

    if (!executed.empty()) {
        std::scoped_lock lock(m_eventMutex);
        m_eventsSinceLastSnapshot.insert(
            m_eventsSinceLastSnapshot.end(),
            std::make_move_iterator(executed.begin()),
            std::make_move_iterator(executed.end()));
    }
}

} // namespace genesis::runtime
