#include "genesis/core/Engine.hpp"

#include <chrono>
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
    : m_clock(SimulationClock::duration{500})
    , m_needSatisfier({})
    , m_resourceSystem(m_world, m_eventBus) {
    spdlog::info("GenesisEngine core initialized");
    loadInitialWorld();
    m_resourceSystem.initialize(m_registry);
    configureNeedDefaults();
    spawnDemoAgents();
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
    const float deltaSeconds = std::chrono::duration<float>(m_clock.stepDuration()).count();
    m_resourceSystem.tick(m_registry, stepIndex);
    m_needSystem.update(m_registry, deltaSeconds);
    m_needSatisfier.update(m_registry, m_resourceSystem);
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

void Engine::configureNeedDefaults() {
    using genesis::agents::NeedDescriptor;
    using genesis::agents::NeedType;

    NeedDescriptor hunger{};
    hunger.type = NeedType::Hunger;
    hunger.minValue = 0.0f;
    hunger.maxValue = 100.0f;
    hunger.decayPerSecond = 0.03f; // reaches critical in roughly 45 minutes real-time at default step
    hunger.satisfiedThreshold = 20.0f;
    hunger.criticalThreshold = 75.0f;
    m_needSystem.setDefaultDescriptor(hunger);

    NeedDescriptor energy{};
    energy.type = NeedType::Energy;
    energy.minValue = 0.0f;
    energy.maxValue = 100.0f;
    energy.decayPerSecond = 0.015f;
    energy.satisfiedThreshold = 10.0f;
    energy.criticalThreshold = 70.0f;
    m_needSystem.setDefaultDescriptor(energy);

    NeedDescriptor social{};
    social.type = NeedType::Social;
    social.minValue = 0.0f;
    social.maxValue = 100.0f;
    social.decayPerSecond = 0.02f;
    social.satisfiedThreshold = 15.0f;
    social.criticalThreshold = 60.0f;
    m_needSystem.setDefaultDescriptor(social);
}

void Engine::spawnDemoAgents() {
    using genesis::agents::NeedType;

    auto entity = m_registry.create();
    auto& needs = m_registry.emplace<genesis::agents::NeedComponent>(entity);
    m_needSystem.applyDefaults(needs);

    needs.needs.setState(NeedType::Hunger, 10.0f);
    needs.needs.setState(NeedType::Energy, 25.0f);
    needs.needs.setState(NeedType::Social, 5.0f);

    spdlog::info("Spawned demo agent with baseline needs");
}

Engine::ResourceRequestResult Engine::requestResource(world::ResourceType type, std::uint32_t amount, world::LocationId preferredLocation) {
    Engine::ResourceRequestResult result{};
    result.requested = amount;

    if (amount == 0U) {
        return result;
    }

    const auto consumed = m_resourceSystem.consume(m_registry, type, amount, preferredLocation);
    result.fulfilled = consumed;
    return result;
}
} // namespace genesis::core
