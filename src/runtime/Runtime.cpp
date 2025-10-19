#include "genesis/runtime/Runtime.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <random>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "genesis/worldgen/ConfigLoader.hpp"
#include "genesis/worldgen/Generator.hpp"

namespace genesis::runtime {

Runtime::Runtime(RuntimeConfig config)
    : m_config(std::move(config))
    , m_engine() {
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
        auto result = generateWorldFromConfig(m_config.worldgen->configPath, m_config.worldgen->seedOverride);
        if (!result.success) {
            spdlog::error("Initial world generation failed: {}", result.error);
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

const genesis::telemetry::TickTelemetry* Runtime::latestSnapshot() const noexcept {
    return m_engine.latestTelemetry();
}

Runtime::WorldGenerationResult Runtime::generateWorldFromConfig(const std::filesystem::path& configPath, std::optional<std::uint64_t> seedOverride) {
    WorldGenerationResult result{};
    result.configPath = std::filesystem::absolute(configPath);

    if (configPath.empty()) {
        result.error = "配置路径为空";
        spdlog::error("World generation failed: {}", result.error);
        m_lastWorldGen = result;
        return *m_lastWorldGen;
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
        m_engine.reloadWorld(std::move(generated.world_graph));
        result.success = true;
        result.error.clear();
        m_lastSeed = result.seed;
        spdlog::info("World generation succeeded: seed={} locations={} edges={} duration={:.2f}ms",
            result.seed.value, result.locationCount, result.edgeCount, result.durationMs);
    } catch (const std::exception& ex) {
        result.success = false;
        result.error = ex.what();
        m_lastSeed.reset();
        spdlog::error("World generation failed: {}", result.error);
    }

    m_lastWorldGen = result;
    return *m_lastWorldGen;
}

std::unique_ptr<Runtime> createRuntime(RuntimeConfig config) {
    return std::make_unique<Runtime>(std::move(config));
}

} // namespace genesis::runtime
