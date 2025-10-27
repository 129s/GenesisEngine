#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <entt/entt.hpp>

#include "genesis/agents/ActionSystem.hpp"
#include "genesis/agents/NeedSatisfier.hpp"
#include "genesis/agents/NeedSystem.hpp"
#include "genesis/messaging/EventBus.hpp"
#include "genesis/simulation/Movement2DSystem.hpp"
#include "genesis/simulation/Scheduler.hpp"
#include "genesis/telemetry/TelemetryBuffer.hpp"
#include "genesis/world/WorldDatabase.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

namespace genesis::core { class Engine; }

namespace genesis::simulation {

class SimulationContext {
public:
    SimulationContext();

    void setWorldDatabase(std::shared_ptr<world::WorldDatabase> database, entt::registry& registry);
    void tick(entt::registry& registry, float deltaSeconds, std::uint64_t stepIndex);
    void reset(entt::registry& registry);

    [[nodiscard]] bool hasWorld() const noexcept { return static_cast<bool>(m_worldDatabase); }
    [[nodiscard]] const std::shared_ptr<world::WorldDatabase>& worldDatabase() const noexcept { return m_worldDatabase; }

    [[nodiscard]] world::system::ResourceSystem* resourceSystem() noexcept { return m_resourceSystem.get(); }
    [[nodiscard]] const world::system::ResourceSystem* resourceSystem() const noexcept { return m_resourceSystem.get(); }
    [[nodiscard]] agents::ActionExecutor* actionExecutor() noexcept { return m_actionExecutor.get(); }

    void collectResourceSnapshots(const entt::registry& registry, std::vector<telemetry::ResourceSnapshot>& out) const;

    messaging::EventBus& eventBus() noexcept { return m_eventBus; }
    const messaging::EventBus& eventBus() const noexcept { return m_eventBus; }

private:
    friend class genesis::core::Engine;

    void bindScheduler();
    void rebuildResourceSystem(entt::registry& registry);

    std::shared_ptr<world::WorldDatabase> m_worldDatabase;
    messaging::EventBus m_eventBus;
    agents::NeedSystem m_needSystem;
    agents::NeedSatisfier m_needSatisfier;
    std::unique_ptr<agents::ActionExecutor> m_actionExecutor;
    std::unique_ptr<world::system::ResourceSystem> m_resourceSystem;
    Movement2DSystem m_movementSystem;
    Scheduler m_scheduler;
};

} // namespace genesis::simulation
