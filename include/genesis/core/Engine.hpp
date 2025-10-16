#pragma once

#include <cstdint>
#include <vector>

#include <entt/entt.hpp>

#include "genesis/core/SimulationClock.hpp"
#include "genesis/messaging/EventBus.hpp"
#include "genesis/agents/NeedSystem.hpp"
#include "genesis/agents/AgentComponents.hpp"
#include "genesis/agents/NeedSatisfier.hpp"
#include "genesis/world/WorldRegistry.hpp"
#include "genesis/world/system/ResourceSystem.hpp"
#include "genesis/telemetry/TelemetryBuffer.hpp"
#include "genesis/planner/HungerPlanner.hpp"

namespace genesis::core {

class Engine {
public:
    struct ResourceRequestResult {
        std::uint32_t fulfilled{0};
        std::uint32_t requested{0};
    };

    Engine();

    void run(std::uint64_t maxSteps);

    [[nodiscard]] SimulationClock& clock() noexcept { return m_clock; }
    [[nodiscard]] const SimulationClock& clock() const noexcept { return m_clock; }

    [[nodiscard]] messaging::EventBus& eventBus() noexcept { return m_eventBus; }
    [[nodiscard]] const messaging::EventBus& eventBus() const noexcept { return m_eventBus; }

    [[nodiscard]] world::WorldRegistry& world() noexcept { return m_world; }
    [[nodiscard]] const world::WorldRegistry& world() const noexcept { return m_world; }

    ResourceRequestResult requestResource(world::ResourceType type, std::uint32_t amount, world::LocationId preferredLocation);

    [[nodiscard]] const telemetry::TelemetryBuffer& telemetry() const noexcept { return m_telemetry; }

private:
    void processStep(std::uint64_t stepIndex);
    void loadInitialWorld();
    void configureNeedDefaults();
    void spawnDemoAgents();
    void captureTelemetry(std::uint64_t stepIndex);
    void reportTelemetry(std::uint64_t stepIndex);

    SimulationClock m_clock;
    messaging::EventBus m_eventBus;
    world::WorldRegistry m_world;
    entt::registry m_registry;
    agents::NeedSystem m_needSystem;
    planner::HungerPlanner m_hungerPlanner;
    world::system::ResourceSystem m_resourceSystem;
    telemetry::TelemetryBuffer m_telemetry;
    std::vector<planner::HungerDecision> m_hungerDecisions;
    std::uint64_t m_lastTelemetryReportStep{0};
    static constexpr std::uint64_t kTelemetryReportInterval = 120;
};

} // namespace genesis::core
