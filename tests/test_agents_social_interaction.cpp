#include <gtest/gtest.h>

#include <cstdint>

#include <entt/entt.hpp>

#include "genesis/agents/ActionSystem.hpp"
#include "genesis/agents/Movement2D.hpp"
#include "genesis/agents/NeedSystem.hpp"
#include "genesis/messaging/EventBus.hpp"
#include "genesis/world/WorldDatabase.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

namespace genesis::tests {

TEST(AgentsSocialInteraction, SocializeReducesSocialNeed) {
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

    genesis::agents::NeedComponent needsA{};
    genesis::agents::NeedDescriptor social{};
    social.type = genesis::agents::NeedType::Social;
    social.minValue = 0.0f;
    social.maxValue = 100.0f;
    social.satisfiedThreshold = 30.0f;
    social.criticalThreshold = 80.0f;
    needsA.needs.setDescriptor(social);
    needsA.needs.setState(genesis::agents::NeedType::Social, 90.0f);
    registry.emplace<genesis::agents::NeedComponent>(a, needsA);

    genesis::agents::NeedComponent needsB{};
    needsB.needs.setDescriptor(social);
    needsB.needs.setState(genesis::agents::NeedType::Social, 90.0f);
    registry.emplace<genesis::agents::NeedComponent>(b, needsB);

    actions.requestSocialize(a,
                             static_cast<std::uint32_t>(entt::to_integral(b)),
                             3,
                             20.0f,
                             registry);

    actions.requestSocialize(b,
                             static_cast<std::uint32_t>(entt::to_integral(a)),
                             3,
                             20.0f,
                             registry);

    for (int i = 0; i < 8; ++i) {
        actions.update(registry, 0.5f);
    }

    const auto& outA = registry.get<genesis::agents::NeedComponent>(a);
    const auto& outB = registry.get<genesis::agents::NeedComponent>(b);
    EXPECT_LT(outA.needs.state(genesis::agents::NeedType::Social)->value, 90.0f);
    EXPECT_LT(outB.needs.state(genesis::agents::NeedType::Social)->value, 90.0f);
}

} // namespace genesis::tests

