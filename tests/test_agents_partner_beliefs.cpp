#include <gtest/gtest.h>

#include <cstdint>

#include <entt/entt.hpp>

#include "genesis/agents/ActionSystem.hpp"
#include "genesis/agents/Beliefs.hpp"
#include "genesis/agents/LearningSystem.hpp"
#include "genesis/agents/Movement2D.hpp"
#include "genesis/agents/NeedSatisfier.hpp"
#include "genesis/agents/NeedSystem.hpp"
#include "genesis/messaging/EventBus.hpp"
#include "genesis/world/WorldDatabase.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

namespace genesis::tests {

TEST(LearningSystemBeliefs, UpdatesPartnerMeetReliabilityEma) {
    entt::registry registry;

    const auto a = registry.create();
    const auto b = registry.create();

    registry.emplace<genesis::agents::components::AgentBeliefs>(a);
    registry.emplace<genesis::agents::components::AgentBeliefs>(b);

    auto& outA = registry.emplace<genesis::agents::components::AgentOutcomeBuffer>(a);
    outA.socialInteractions.push_back(genesis::agents::SocialInteractionOutcome{
        static_cast<std::uint32_t>(entt::to_integral(b)),
        false,
        genesis::agents::SocialInteractionFailure::Reject});
    outA.socialInteractions.push_back(genesis::agents::SocialInteractionOutcome{
        static_cast<std::uint32_t>(entt::to_integral(b)),
        true,
        genesis::agents::SocialInteractionFailure::None});

    genesis::agents::LearningSystemConfig cfg{};
    cfg.beliefPartnerForgetPerSecond = 0.0f;
    cfg.beliefPartnerReliabilityAlpha = 1.0f;
    cfg.socialBeliefShareStrength = 0.0f;

    genesis::agents::LearningSystem learning(cfg);
    learning.update(registry, 0.0f);

    const auto& beliefs = registry.get<genesis::agents::components::AgentBeliefs>(a);
    const auto bKey = static_cast<std::uint32_t>(entt::to_integral(b));
    ASSERT_TRUE(beliefs.partners.contains(bKey));
    EXPECT_NEAR(beliefs.partners.at(bKey).meetReliabilityEma, 1.0f, 1e-6f);
}

TEST(NeedSatisfierBeliefs, PrefersReliableSocialPartner) {
    genesis::world::InMemoryWorldDatabase db;
    db.addMap(genesis::world::Map{.id = 1, .name = "m1"});

    entt::registry registry;
    genesis::messaging::EventBus eventBus;
    genesis::world::system::ResourceSystem resources(db, eventBus);
    resources.initialize(registry);

    genesis::agents::ActionExecutor actions(db, resources);

    const auto a = registry.create();
    const auto b = registry.create();
    const auto c = registry.create();

    registry.emplace<genesis::agents::components::AgentLocation2D>(a, genesis::agents::components::AgentLocation2D{1, 0.0f, 0.0f});
    registry.emplace<genesis::agents::components::AgentLocation2D>(b, genesis::agents::components::AgentLocation2D{1, 0.0f, 0.0f});
    registry.emplace<genesis::agents::components::AgentLocation2D>(c, genesis::agents::components::AgentLocation2D{1, 0.0f, 0.0f});

    genesis::agents::NeedComponent needsA{};
    genesis::agents::NeedDescriptor social{};
    social.type = genesis::agents::NeedType::Social;
    social.minValue = 0.0f;
    social.maxValue = 100.0f;
    social.satisfiedThreshold = 30.0f;
    social.criticalThreshold = 80.0f;
    needsA.needs.setDescriptor(social);
    needsA.needs.setState(genesis::agents::NeedType::Social, 95.0f);
    registry.emplace<genesis::agents::NeedComponent>(a, needsA);

    genesis::agents::NeedComponent needsB{};
    needsB.needs.setDescriptor(social);
    needsB.needs.setState(genesis::agents::NeedType::Social, 95.0f);
    registry.emplace<genesis::agents::NeedComponent>(b, needsB);

    genesis::agents::NeedComponent needsC{};
    needsC.needs.setDescriptor(social);
    needsC.needs.setState(genesis::agents::NeedType::Social, 95.0f);
    registry.emplace<genesis::agents::NeedComponent>(c, needsC);

    auto& beliefsA = registry.emplace<genesis::agents::components::AgentBeliefs>(a);
    const auto bKey = static_cast<std::uint32_t>(entt::to_integral(b));
    const auto cKey = static_cast<std::uint32_t>(entt::to_integral(c));
    beliefsA.partners[bKey].meetReliabilityEma = 1.0f;
    beliefsA.partners[cKey].meetReliabilityEma = 0.0f;

    genesis::agents::NeedSatisfier satisfier(genesis::agents::NeedSatisfierConfig{});
    satisfier.update(registry, db, resources, 0, &actions);

    const auto* queue = registry.try_get<genesis::agents::ActionQueue>(a);
    ASSERT_NE(queue, nullptr);
    ASSERT_FALSE(queue->tasks.empty());
    EXPECT_EQ(queue->tasks.front().type, genesis::agents::ActionType::SocializeWithAgent);
    EXPECT_EQ(queue->tasks.front().targetEntityId, bKey);
}

} // namespace genesis::tests
