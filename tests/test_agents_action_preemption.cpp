#include <gtest/gtest.h>

#include <cstdint>

#include <entt/entt.hpp>
#include <nlohmann/json.hpp>

#include "genesis/agents/ActionSystem.hpp"
#include "genesis/agents/CarriedResources.hpp"
#include "genesis/agents/Experience.hpp"
#include "genesis/agents/Movement2D.hpp"
#include "genesis/agents/NeedSystem.hpp"
#include "genesis/messaging/EventBus.hpp"
#include "genesis/world/WorldDatabase.hpp"
#include "genesis/world/system/ResourceSystem.hpp"

namespace genesis::tests {

TEST(ActionExecutorPreemption, PausesWorkshopJobToSatisfyCriticalNeed) {
    using json = nlohmann::json;

    genesis::world::InMemoryWorldDatabase db;
    db.addMap(genesis::world::Map{.id = 1, .name = "m1"});

    genesis::world::Interaction foodSource{};
    foodSource.id = 2000;
    foodSource.mapId = 1;
    foodSource.sceneId = 0;
    foodSource.kind = genesis::world::InteractionKind::Resource;
    foodSource.coordLocal = {0, 0};
    foodSource.coordGlobal = {0, 0};
    foodSource.name = "food_source";
    foodSource.resourceType = genesis::world::ResourceType::Food;
    foodSource.capacity = 10;
    foodSource.regenPerStep = 0;
    db.addInteraction(foodSource);

    genesis::world::Interaction workshop{};
    workshop.id = 1000;
    workshop.mapId = 1;
    workshop.sceneId = 0;
    workshop.kind = genesis::world::InteractionKind::Resource;
    workshop.coordLocal = {0, 0};
    workshop.coordGlobal = {0, 0};
    workshop.name = "tool_workshop";
    workshop.resourceType = genesis::world::ResourceType::Tool;
    workshop.capacity = 50;
    workshop.regenPerStep = 0;
    workshop.meta = json::object(
        {{"workshop",
          {{"initial", 0},
           {"workTicksPerBatch", 5},
           {"slots", 1},
           {"recipes",
            json::array(
                {json::object({{"outputUnits", 1},
                               {"inputs", json::array({json::object({{"type", "Ore"}, {"units", 1}})})}})})}}}});
    db.addInteraction(workshop);

    entt::registry registry;
    genesis::messaging::EventBus eventBus;
    genesis::world::system::ResourceSystem resources(db, eventBus);
    resources.initialize(registry);

    genesis::agents::ActionExecutor actions(db, resources);

    const auto agent = registry.create();
    registry.emplace<genesis::agents::components::AgentLocation2D>(agent, genesis::agents::components::AgentLocation2D{1, 0.0f, 0.0f});

    genesis::agents::NeedComponent needs{};
    genesis::agents::NeedDescriptor hunger{};
    hunger.type = genesis::agents::NeedType::Hunger;
    hunger.minValue = 0.0f;
    hunger.maxValue = 100.0f;
    hunger.satisfiedThreshold = 30.0f;
    hunger.criticalThreshold = 80.0f;
    needs.needs.setDescriptor(hunger);
    needs.needs.setState(genesis::agents::NeedType::Hunger, 10.0f);
    registry.emplace<genesis::agents::NeedComponent>(agent, needs);

    auto& carried = registry.emplace<genesis::agents::components::CarriedResources>(agent);
    carried.add(genesis::world::ResourceType::Ore, 10);

    genesis::agents::ActionQueue queue{};
    genesis::agents::ActionTask prod{};
    prod.type = genesis::agents::ActionType::ProduceResource;
    prod.interaction = 1000;
    prod.resource = genesis::world::ResourceType::Tool;
    prod.batches = 1;
    queue.tasks.push_back(prod);
    registry.emplace<genesis::agents::ActionQueue>(agent, std::move(queue));

    actions.update(registry, 0.0f);
    ASSERT_TRUE(registry.any_of<genesis::agents::ActionQueue>(agent));

    auto& mutableNeeds = registry.get<genesis::agents::NeedComponent>(agent);
    mutableNeeds.needs.setState(genesis::agents::NeedType::Hunger, 90.0f);

    actions.update(registry, 0.0f);

    std::vector<genesis::telemetry::WorkshopAttemptSnapshot> attempts;
    actions.drainWorkshopAttemptSnapshots(attempts);
    EXPECT_TRUE(attempts.empty());

    const auto& q = registry.get<genesis::agents::ActionQueue>(agent);
    ASSERT_FALSE(q.tasks.empty());
    EXPECT_EQ(q.tasks.front().type, genesis::agents::ActionType::ProduceResource);

    std::vector<genesis::telemetry::ResourceAttemptSnapshot> resourceAttempts;
    actions.drainResourceAttemptSnapshots(resourceAttempts);
    ASSERT_FALSE(resourceAttempts.empty());
    bool ate = false;
    for (const auto& ra : resourceAttempts) {
        if (ra.action == "ConsumeResource" && ra.interactionId == 2000 && ra.obtainedUnits > 0U) {
            ate = true;
            break;
        }
    }
    EXPECT_TRUE(ate);

    const auto* exp = registry.try_get<genesis::agents::components::AgentExperience>(agent);
    ASSERT_NE(exp, nullptr);
    const auto idx = genesis::agents::needIndex(genesis::agents::NeedType::Hunger);
    EXPECT_GT(exp->bufferMultiplier[idx], 1.0f);
}

} // namespace genesis::tests
