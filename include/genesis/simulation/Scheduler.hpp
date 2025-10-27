#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace genesis::agents {
class NeedSystem;
class NeedSatisfier;
class ActionExecutor;
}

namespace genesis::simulation {

class Movement2DSystem;
}

namespace genesis::world::system {
class ResourceSystem;
}

namespace genesis::simulation {

class Scheduler {
public:
    Scheduler() = default;

    void setNeedSystem(genesis::agents::NeedSystem* system) noexcept { m_needSystem = system; }
    void setNeedSatisfier(genesis::agents::NeedSatisfier* satisfier) noexcept { m_needSatisfier = satisfier; }
    void setActionExecutor(genesis::agents::ActionExecutor* executor) noexcept { m_actionExecutor = executor; }
    void setMovementSystem(Movement2DSystem* movement) noexcept { m_movementSystem = movement; }
    void setResourceSystem(genesis::world::system::ResourceSystem* resource) noexcept { m_resourceSystem = resource; }

    void update(entt::registry& registry, float deltaSeconds, std::uint64_t stepIndex) const;

private:
    genesis::agents::NeedSystem* m_needSystem{nullptr};
    genesis::agents::NeedSatisfier* m_needSatisfier{nullptr};
    genesis::agents::ActionExecutor* m_actionExecutor{nullptr};
    Movement2DSystem* m_movementSystem{nullptr};
    genesis::world::system::ResourceSystem* m_resourceSystem{nullptr};
};

} // namespace genesis::simulation
