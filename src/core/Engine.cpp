#include "genesis/core/Engine.hpp"

#include <filesystem>

#include <spdlog/spdlog.h>

#include "genesis/world/WorldBootstrap.hpp"
#include "genesis/world/WorldLoader.hpp"

namespace genesis::core {

namespace {

std::filesystem::path findDataFile(const std::filesystem::path& relative) {
    constexpr int searchDepth = 4;
    auto current = std::filesystem::current_path();

    for (int i = 0; i < searchDepth; ++i) {
        const auto candidate = current / relative;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
        if (current.has_parent_path()) {
            current = current.parent_path();
        } else {
            break;
        }
    }

    return {};
}

} // namespace

Engine::Engine()
    : m_clock(SimulationClock::duration{500}) {
    spdlog::info("GenesisEngine core initialized");
    loadInitialWorld();
}

void Engine::run(std::uint64_t maxSteps) {
    const auto delta = m_clock.stepDuration();
    std::uint64_t processed = 0;

    while (processed < maxSteps) {
        m_clock.advance(delta);

        if (!m_clock.stepReady()) {
            continue;
        }

        const auto current = m_clock.consumeStep();
        processStep(current);
        ++processed;
    }
}

void Engine::processStep(std::uint64_t stepIndex) {
    spdlog::debug("Processing simulation step {}", stepIndex);
    eventBus().updateAll();
}

void Engine::loadInitialWorld() {
    static const std::filesystem::path defaultWorld{"data/world/demo_world.json"};

    if (const auto located = findDataFile(defaultWorld); !located.empty()) {
        spdlog::info("Loading world from {}", located.string());
        const auto result = genesis::world::loadWorldFromFile(located, m_world);
        if (result.success) {
            spdlog::info("World loaded ({} locations, {} spawns)",
                m_world.locationCount(), m_world.resourceSpawnCount());
            return;
        }

        spdlog::error("Failed to load world file: {}", result.error);
    } else {
        spdlog::warn("World file {} not found, using built-in demo world", defaultWorld.string());
    }

    m_world.setGraph(genesis::world::createDemoWorldGraph());
    spdlog::info("Fallback demo world loaded ({} locations, {} spawns)",
        m_world.locationCount(), m_world.resourceSpawnCount());
}

} // namespace genesis::core
