#include <gtest/gtest.h>

#include <cstdint>

#include <entt/entt.hpp>

#include "genesis/agents/ActionSystem.hpp"
#include "genesis/agents/Meetings.hpp"
#include "genesis/agents/NeedSatisfier.hpp"
#include "genesis/agents/NeedSystem.hpp"
#include "genesis/messaging/EventBus.hpp"
#include "genesis/world/WorldDatabase.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

namespace genesis::tests {

TEST(AgentsSocialNegotiation, ProposeAcceptMeetThenSocialize) {
    genesis::world::InMemoryWorldDatabase db;
    db.addMap(genesis::world::Map{.id = 1, .name = "m1"});
    db.addInteraction(genesis::world::Interaction{
        .id = 10,
        .mapId = 1,
        .sceneId = 0,
        .kind = genesis::world::InteractionKind::Landmark,
        .coordLocal = {0, 0},
        .coordGlobal = std::make_pair(0, 0),
        .name = "meet",
        .meta = std::nullopt,
    });

    entt::registry registry;
    genesis::messaging::EventBus eventBus;
    genesis::world::system::ResourceSystem resources(db, eventBus);
    resources.initialize(registry);

    genesis::agents::ActionExecutor actions(db, resources);

    genesis::agents::NeedSatisfierConfig cfg{};
    cfg.socialPrepareMargin = 0.0f;
    cfg.socialInteractTicksPerUnit = 1;
    cfg.socialReliefPerUnit = 20.0f;
    cfg.socialMeetProposalTtlSteps = 32;

    genesis::agents::NeedSatisfier satisfier(cfg);

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

    // Step 0: A proposes, B accepts.
    satisfier.update(registry, db, resources, 0, &actions);
    actions.update(registry, 0.0f);

    const auto* meetA0 = registry.try_get<genesis::agents::components::AgentSocialMeetState>(a);
    const auto* meetB0 = registry.try_get<genesis::agents::components::AgentSocialMeetState>(b);
    ASSERT_NE(meetA0, nullptr);
    ASSERT_NE(meetB0, nullptr);
    EXPECT_EQ(meetA0->status, genesis::agents::components::SocialMeetStatus::Accepted);
    EXPECT_EQ(meetB0->status, genesis::agents::components::SocialMeetStatus::Accepted);

    // Step 1: rendezvous resolved -> a single socialize action is scheduled and executed.
    satisfier.update(registry, db, resources, 1, &actions);
    for (int i = 0; i < 6; ++i) {
        actions.update(registry, 0.5f);
    }

    const auto& outA = registry.get<genesis::agents::NeedComponent>(a);
    const auto& outB = registry.get<genesis::agents::NeedComponent>(b);
    EXPECT_LT(outA.needs.state(genesis::agents::NeedType::Social)->value, 90.0f);
    EXPECT_LT(outB.needs.state(genesis::agents::NeedType::Social)->value, 90.0f);
}

} // namespace genesis::tests
