#pragma once

#include <cstdint>
#include <functional>

#include <entt/entt.hpp>

#include "genesis/agents/NeedSystem.hpp"
#include "genesis/world/system/ResourceSystem.hpp"
#include "genesis/world/WorldDatabase.hpp"
#include "genesis/agents/Movement2D.hpp"

namespace genesis::agents {

class ActionExecutor;

struct NeedSatisfierConfig {
    float crossMapPenalty{500.0f};
    float demandPenaltyPerAgent{60.0f};
    float switchScoreMargin{50.0f};
    float stockoutRiskPenalty{80.0f};

    std::uint32_t hungerUnitsPerRequest{2};
    float hungerReliefPerUnit{12.0f};
    float hungerPrepareMargin{5.0f};
    std::function<world::InteractionId(entt::entity)> hungerPreferredLocator{};

    std::uint32_t thirstUnitsPerRequest{2};
    float thirstReliefPerUnit{12.0f};
    float thirstPrepareMargin{5.0f};
    std::function<world::InteractionId(entt::entity)> thirstPreferredLocator{};

    std::uint32_t socialUnitsPerRequest{1};
    float socialReliefPerUnit{20.0f};
    float socialPrepareMargin{5.0f};
    std::function<world::InteractionId(entt::entity)> socialPreferredLocator{};
};

class NeedSatisfier {
public:
    explicit NeedSatisfier(NeedSatisfierConfig config = {});

    void update(entt::registry& registry,
                world::WorldDatabase& db,
                world::system::ResourceSystem& resourceSystem,
                std::uint64_t stepIndex,
                ActionExecutor* actionExecutor = nullptr) const;

private:
    NeedSatisfierConfig m_config;
};

} // namespace genesis::agents
