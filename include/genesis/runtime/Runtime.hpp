#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "genesis/core/Engine.hpp"
#include "genesis/runtime/SimulationSnapshot.hpp"
#include "genesis/runtime/SnapshotDiff.hpp"
#include "genesis/telemetry/TelemetryBuffer.hpp"
#include "genesis/world/WorldLoader.hpp"
#include "genesis/world/WorldTypes.hpp"
#include "genesis/worldgen/Types.hpp"
#include "genesis/runtime/RuntimeEvents.hpp"

namespace genesis::runtime {

struct RuntimeConfig {
    std::uint64_t bootstrapSteps{0};

    struct InitialWorldgen {
        bool autoGenerate{false};
        std::filesystem::path configPath{};
        std::optional<std::uint64_t> seedOverride;
        std::optional<std::filesystem::path> outputPath;
    };

    std::optional<InitialWorldgen> worldgen;
    std::optional<std::filesystem::path> initialWorldPath;
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

    [[nodiscard]] const SimulationSnapshot* latestSnapshot() const noexcept;
    [[nodiscard]] std::optional<SimulationSnapshotDiff> latestSnapshotDiff() const noexcept;
    [[nodiscard]] const std::optional<genesis::worldgen::Seed>& lastSeed() const noexcept { return m_lastSeed; }

    struct WorldGenerationResult {
        bool success{false};
        std::filesystem::path configPath{};
        std::optional<std::filesystem::path> outputPath;
        genesis::worldgen::Seed seed{};
        std::size_t locationCount{0};
        std::size_t edgeCount{0};
        double durationMs{0.0};
        std::vector<genesis::worldgen::GenerationLogEntry> logs;
        std::optional<genesis::world::LocationGraph> worldGraph;
        std::string error;
    };

    WorldGenerationResult generateWorldFromConfig(const std::filesystem::path& configPath, std::optional<std::uint64_t> seedOverride = std::nullopt, std::optional<std::filesystem::path> outputPath = std::nullopt);
    [[nodiscard]] const std::optional<WorldGenerationResult>& lastWorldGeneration() const noexcept { return m_lastWorldGen; }

    [[nodiscard]] genesis::world::WorldLoadResult loadWorldFromFile(const std::filesystem::path& path);
    [[nodiscard]] genesis::world::WorldSaveResult saveWorldToFile(const std::filesystem::path& path) const;

    [[nodiscard]] genesis::core::Engine& engine() noexcept { return m_engine; }
    [[nodiscard]] const genesis::core::Engine& engine() const noexcept { return m_engine; }

    void enqueueEvent(RuntimeEvent event);

private:
    void drainPendingEvents();

    RuntimeConfig m_config;
    genesis::core::Engine m_engine;
    std::optional<genesis::worldgen::Seed> m_lastSeed;
    std::optional<WorldGenerationResult> m_lastWorldGen;
    std::atomic<std::uint64_t> m_snapshotVersion{0};
    SimulationSnapshotBuffer m_snapshotBuffer;
    std::atomic<std::uint64_t> m_nextEventId{1};
    mutable std::mutex m_eventMutex;
    std::queue<RuntimeEvent> m_pendingEvents;
    std::vector<RuntimeEventReport> m_eventsSinceLastSnapshot;
};

std::unique_ptr<Runtime> createRuntime(RuntimeConfig config = {});

} // namespace genesis::runtime
