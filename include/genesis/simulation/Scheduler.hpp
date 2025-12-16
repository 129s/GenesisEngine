#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace genesis::world { class WorldDatabase; }

namespace genesis::agents {
class NeedSystem;
class NeedSatisfier;
class ActionExecutor;
class LearningSystem;
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
    void setLearningSystem(genesis::agents::LearningSystem* learning) noexcept { m_learningSystem = learning; }
    void setMovementSystem(Movement2DSystem* movement) noexcept { m_movementSystem = movement; }
    void setResourceSystem(genesis::world::system::ResourceSystem* resource) noexcept { m_resourceSystem = resource; }
    void setWorldDatabase(genesis::world::WorldDatabase* db) noexcept { m_worldDatabase = db; }

    void update(entt::registry& registry, float deltaSeconds, std::uint64_t stepIndex) const;

private:
    genesis::agents::NeedSystem* m_needSystem{nullptr};
    genesis::agents::NeedSatisfier* m_needSatisfier{nullptr};
    genesis::agents::ActionExecutor* m_actionExecutor{nullptr};
    genesis::agents::LearningSystem* m_learningSystem{nullptr};
    Movement2DSystem* m_movementSystem{nullptr};
    genesis::world::system::ResourceSystem* m_resourceSystem{nullptr};
    genesis::world::WorldDatabase* m_worldDatabase{nullptr};
};

} // namespace genesis::simulation
