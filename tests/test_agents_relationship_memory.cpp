#include <gtest/gtest.h>

#include <cstdint>

#include <entt/entt.hpp>

#include "genesis/agents/ActionSystem.hpp"
#include "genesis/agents/LearningSystem.hpp"
#include "genesis/agents/Movement2D.hpp"
#include "genesis/agents/NeedSystem.hpp"
#include "genesis/agents/Relations.hpp"
#include "genesis/messaging/EventBus.hpp"
#include "genesis/world/WorldDatabase.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

namespace genesis::tests {

TEST(AgentsRelationshipMemory, BondsFormViaSuccessfulSocialize) {
    genesis::world::InMemoryWorldDatabase db;
    db.addMap(genesis::world::Map{.id = 1, .name = "m1"});

    entt::registry registry;
    genesis::messaging::EventBus eventBus;
    genesis::world::system::ResourceSystem resources(db, eventBus);
    resources.initialize(registry);

    genesis::agents::ActionExecutor actions(db, resources);

    const auto a = registry.create();
    const auto b = registry.create();

    registry.emplace<genesis::agents::components::AgentLocation2D>(a, genesis::agents::components::AgentLocation2D{1, 0.0f, 0.0f});
    registry.emplace<genesis::agents::components::AgentLocation2D>(b, genesis::agents::components::AgentLocation2D{1, 0.0f, 0.0f});

    genesis::agents::NeedDescriptor social{};
    social.type = genesis::agents::NeedType::Social;
    social.minValue = 0.0f;
    social.maxValue = 100.0f;
    social.satisfiedThreshold = 30.0f;
    social.criticalThreshold = 80.0f;

    genesis::agents::NeedComponent needsA{};
    needsA.needs.setDescriptor(social);
    needsA.needs.setState(genesis::agents::NeedType::Social, 90.0f);
    registry.emplace<genesis::agents::NeedComponent>(a, needsA);

    genesis::agents::NeedComponent needsB{};
    needsB.needs.setDescriptor(social);
    needsB.needs.setState(genesis::agents::NeedType::Social, 90.0f);
    registry.emplace<genesis::agents::NeedComponent>(b, needsB);

    actions.requestSocialize(a, static_cast<std::uint32_t>(entt::to_integral(b)), 1, 10.0f, registry);

    for (int i = 0; i < 4; ++i) {
        actions.update(registry, 0.5f);
    }

    genesis::agents::LearningSystemConfig cfg{};
    cfg.socialBeliefShareStrength = 0.0f;
    cfg.relationshipForgetPerSecond = 0.0f;
    cfg.relationshipBondGain = 0.20f;
    cfg.relationshipBondGainReciprocal = 0.10f;
    cfg.relationshipSnubPenalty = 0.30f;
    cfg.relationshipMinAbsToKeep = 0.0f;

    genesis::agents::LearningSystem learning(cfg);
    learning.update(registry, 0.0f);

    const auto& relA = registry.get<genesis::agents::components::AgentRelations>(a);
    const auto& relB = registry.get<genesis::agents::components::AgentRelations>(b);
    const auto bKey = static_cast<std::uint32_t>(entt::to_integral(b));
    const auto aKey = static_cast<std::uint32_t>(entt::to_integral(a));

    ASSERT_TRUE(relA.affinityByPartner.contains(bKey));
    ASSERT_TRUE(relB.affinityByPartner.contains(aKey));
    EXPECT_NEAR(relA.affinityByPartner.at(bKey), 0.20f, 1e-6f);
    EXPECT_NEAR(relB.affinityByPartner.at(aKey), 0.10f, 1e-6f);
}

} // namespace genesis::tests
