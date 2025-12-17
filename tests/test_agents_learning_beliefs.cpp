#include <gtest/gtest.h>

#include <cstdint>

#include <entt/entt.hpp>

#include "genesis/agents/Beliefs.hpp"
#include "genesis/agents/LearningSystem.hpp"
#include "genesis/agents/NeedSatisfier.hpp"
#include "genesis/agents/NeedSystem.hpp"
#include "genesis/agents/Outcomes.hpp"
#include "genesis/agents/Planner.hpp"
#include "genesis/messaging/EventBus.hpp"
#include "genesis/world/WorldDatabase.hpp"
#include "genesis/world/components/ResourceInventory.hpp"
#include "genesis/world/components/ResourceSpawn.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

namespace genesis::tests {

TEST(LearningSystemBeliefs, UpdatesStockoutRiskEma) {
    entt::registry registry;
    const auto agent = registry.create();

    registry.emplace<genesis::agents::components::AgentBeliefs>(agent);
    auto& outcomes = registry.emplace<genesis::agents::components::AgentOutcomeBuffer>(agent);

    genesis::agents::ResourceAttemptOutcome o1{};
    o1.interaction = 2000;
    o1.need = genesis::agents::NeedType::Hunger;
    o1.failure = genesis::agents::ResourceAttemptFailure::Stockout;
    outcomes.resourceAttempts.push_back(o1);

    genesis::agents::LearningSystem learning;
    learning.update(registry, 0.0f);

    auto& beliefs = registry.get<genesis::agents::components::AgentBeliefs>(agent);
    const float afterStockout = beliefs.stockoutRisk(2000);
    EXPECT_GT(afterStockout, beliefs.priorStockoutRisk);

    genesis::agents::ResourceAttemptOutcome o2{};
    o2.interaction = 2000;
    o2.need = genesis::agents::NeedType::Hunger;
    o2.obtainedUnits = 1;
    o2.failure = genesis::agents::ResourceAttemptFailure::None;
    outcomes.resourceAttempts.push_back(o2);

    learning.update(registry, 0.0f);

    const float afterSuccess = beliefs.stockoutRisk(2000);
    EXPECT_LT(afterSuccess, afterStockout);
}

TEST(LearningSystemBeliefs, SharesStockoutRiskViaSocialInteraction) {
    entt::registry registry;
    const auto a = registry.create();
    const auto b = registry.create();

    auto& beliefsA = registry.emplace<genesis::agents::components::AgentBeliefs>(a);
    auto& beliefsB = registry.emplace<genesis::agents::components::AgentBeliefs>(b);

    beliefsA.priorStockoutRisk = 0.15f;
    beliefsB.priorStockoutRisk = 0.15f;

    beliefsA.interactions[2000].stockoutRiskEma = 1.0f;
    beliefsB.interactions[2000].stockoutRiskEma = 0.0f;

    auto& outcomes = registry.emplace<genesis::agents::components::AgentOutcomeBuffer>(a);
    outcomes.socialInteractions.push_back(genesis::agents::SocialInteractionOutcome{
        static_cast<std::uint32_t>(entt::to_integral(b)),
        true,
    });

    genesis::agents::LearningSystemConfig cfg{};
    cfg.socialBeliefShareStrength = 1.0f;
    cfg.socialBeliefShareMinDeviation = 0.0f;
    cfg.socialBeliefShareTopK = 1;
    genesis::agents::LearningSystem learning(cfg);
    learning.update(registry, 0.0f);

    EXPECT_NEAR(beliefsA.stockoutRisk(2000), 0.375f, 1e-5f);
    EXPECT_NEAR(beliefsB.stockoutRisk(2000), 0.625f, 1e-5f);
}

TEST(NeedSatisfierBeliefs, PenalizesHighStockoutRiskTarget) {
    genesis::world::InMemoryWorldDatabase db;
    db.addMap(genesis::world::Map{.id = 1, .name = "m1"});

    genesis::world::Interaction foodA{};
    foodA.id = 2000;
    foodA.mapId = 1;
    foodA.sceneId = 0;
    foodA.kind = genesis::world::InteractionKind::Resource;
    foodA.coordLocal = {0, 0};
    foodA.coordGlobal = {0, 0};
    foodA.name = "food_a";
    foodA.resourceType = genesis::world::ResourceType::Food;
    foodA.capacity = 10;
    foodA.regenPerStep = 0;
    db.addInteraction(foodA);

    genesis::world::Interaction foodB = foodA;
    foodB.id = 2001;
    foodB.name = "food_b";
    db.addInteraction(foodB);

    entt::registry registry;
    genesis::messaging::EventBus eventBus;
    genesis::world::system::ResourceSystem resources(db, eventBus);
    resources.initialize(registry);

    auto spawnView = registry.view<genesis::world::components::ResourceSpawn, genesis::world::components::ResourceInventory>();
    for (auto e : spawnView) {
        auto& spawn = spawnView.get<genesis::world::components::ResourceSpawn>(e);
        auto& inv = spawnView.get<genesis::world::components::ResourceInventory>(e);
        if (spawn.interaction == 2000 || spawn.interaction == 2001) {
            inv.current = 5;
        }
    }

    const auto agent = registry.create();
    registry.emplace<genesis::agents::components::AgentLocation2D>(
        agent,
        genesis::agents::components::AgentLocation2D{1, 0.0f, 0.0f});

    genesis::agents::NeedComponent needs{};
    genesis::agents::NeedDescriptor hunger{};
    hunger.type = genesis::agents::NeedType::Hunger;
    hunger.minValue = 0.0f;
    hunger.maxValue = 100.0f;
    hunger.satisfiedThreshold = 30.0f;
    hunger.criticalThreshold = 80.0f;
    needs.needs.setDescriptor(hunger);
    needs.needs.setState(genesis::agents::NeedType::Hunger, 90.0f);
    registry.emplace<genesis::agents::NeedComponent>(agent, needs);

    auto& beliefs = registry.emplace<genesis::agents::components::AgentBeliefs>(agent);
    beliefs.interactions[2000].stockoutRiskEma = 0.95f;
    beliefs.interactions[2001].stockoutRiskEma = 0.05f;

    genesis::agents::NeedSatisfierConfig cfg{};
    cfg.stockoutRiskPenalty = 200.0f;
    genesis::agents::NeedSatisfier satisfier(cfg);

    satisfier.update(registry, db, resources, 0, nullptr);

    const auto* decision = registry.try_get<genesis::agents::components::PlannerDecision>(agent);
    ASSERT_NE(decision, nullptr);
    EXPECT_EQ(decision->target, 2001U);
}

} // namespace genesis::tests
