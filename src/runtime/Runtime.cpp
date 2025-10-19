#include "genesis/runtime/Runtime.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <random>
#include <utility>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "genesis/world/WorldLoader.hpp"
#include "genesis/worldgen/ConfigLoader.hpp"
#include "genesis/worldgen/Generator.hpp"

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

    try {
        if (!spdlog::default_logger()) {
            auto logger = spdlog::stdout_color_mt("genesis");
            spdlog::set_default_logger(std::move(logger));
            spdlog::set_level(spdlog::level::info);
        }
    } catch (...) {
        // Logging initialization is best-effort in runtime context
    }

    if (m_config.worldgen && m_config.worldgen->autoGenerate && !m_config.worldgen->configPath.empty()) {
        auto result = generateWorldFromConfig(
            m_config.worldgen->configPath,
            m_config.worldgen->seedOverride,
            m_config.worldgen->outputPath);
        if (!result.success) {
            spdlog::error("Initial world generation failed: {}", result.error);
        }
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
    m_engine.step(steps);
}

void Runtime::run(std::uint64_t steps) {
    m_engine.run(steps);
}

const SimulationSnapshot* Runtime::latestSnapshot() const noexcept {
    return m_snapshotBuffer.latest();
}

Runtime::WorldGenerationResult Runtime::generateWorldFromConfig(const std::filesystem::path& configPath, std::optional<std::uint64_t> seedOverride, std::optional<std::filesystem::path> outputPath) {
    WorldGenerationResult result{};
    result.configPath = std::filesystem::absolute(configPath);

    if (configPath.empty()) {
        result.error = "配置路径为空";
        spdlog::error("World generation failed: {}", result.error);
        m_lastWorldGen = result;
        return *m_lastWorldGen;
    }

    auto resolvedOutput = outputPath;
    if (!resolvedOutput && m_config.worldgen && m_config.worldgen->outputPath) {
        resolvedOutput = m_config.worldgen->outputPath;
    }

    try {
        const auto config = genesis::worldgen::load_config(configPath);
        const std::uint64_t seedValue = seedOverride.value_or(static_cast<std::uint64_t>(std::random_device{}()));
        result.seed = genesis::worldgen::Seed{seedValue};
        const auto start = std::chrono::steady_clock::now();
        auto generated = genesis::worldgen::generate_world(config, result.seed);
        const auto end = std::chrono::steady_clock::now();
        result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
        result.locationCount = generated.location_count;
        result.edgeCount = generated.edge_count;
        result.logs = std::move(generated.logs);
        result.worldGraph = std::move(generated.world_graph);
        result.success = true;
        result.error.clear();

        if (resolvedOutput) {
            const auto absoluteOut = std::filesystem::absolute(*resolvedOutput);
            const auto saveResult = genesis::world::saveWorldToFile(absoluteOut, *result.worldGraph);
            if (!saveResult.success) {
                result.success = false;
                result.error = saveResult.error;
                spdlog::error("Failed to save generated world to {}: {}", absoluteOut.string(), result.error);
            } else {
                result.outputPath = absoluteOut;
                result.worldGraph.reset();
                spdlog::info("Generated world saved to {}", absoluteOut.string());
            }
        }

        if (result.success) {
            m_lastSeed = result.seed;
            spdlog::info("World generation succeeded: seed={} locations={} edges={} duration={:.2f}ms",
                result.seed.value, result.locationCount, result.edgeCount, result.durationMs);
        } else {
            m_lastSeed.reset();
        }
    } catch (const std::exception& ex) {
        result.success = false;
        result.error = ex.what();
        result.worldGraph.reset();
        m_lastSeed.reset();
        spdlog::error("World generation failed: {}", result.error);
    }

    m_lastWorldGen = result;
    return *m_lastWorldGen;
}

genesis::world::WorldLoadResult Runtime::loadWorldFromFile(const std::filesystem::path& path) {
    const auto absolute = std::filesystem::absolute(path);
    auto result = m_engine.loadWorldFromFile(absolute);
    if (result.success) {
        spdlog::info("Runtime loaded world from {}", absolute.string());
    }
    return result;
}

genesis::world::WorldSaveResult Runtime::saveWorldToFile(const std::filesystem::path& path) const {
    const auto absolute = std::filesystem::absolute(path);
    const auto graph = m_engine.exportWorldGraph();
    auto result = genesis::world::saveWorldToFile(absolute, graph);
    if (result.success) {
        spdlog::info("Runtime saved world to {}", absolute.string());
    } else {
        spdlog::error("Failed to save world to {}: {}", absolute.string(), result.error);
    }
    return result;
}

std::unique_ptr<Runtime> createRuntime(RuntimeConfig config) {
    return std::make_unique<Runtime>(std::move(config));
}

} // namespace genesis::runtime
