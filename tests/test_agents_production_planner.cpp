#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "genesis/agents/ProductionPlanner.hpp"
#include "genesis/world/WorldDatabase.hpp"
#include "genesis/world/ResourceTypeStrings.hpp"

TEST(ProductionPlannerTest, BuildsRecoveryPlanForWorkshopDemand) {
    using json = nlohmann::json;

    genesis::world::InMemoryWorldDatabase db;
    db.addMap(genesis::world::Map{1, "map_1", std::nullopt});
    db.addScene(genesis::world::Scene{100, 1, std::nullopt, "scene_1", std::nullopt, std::nullopt, std::nullopt});

    genesis::world::Interaction water{};
    water.id = 1000;
    water.mapId = 1;
    water.sceneId = 100;
    water.kind = genesis::world::InteractionKind::Resource;
    water.coordLocal = {0, 0};
    water.coordGlobal = water.coordLocal;
    water.name = "water_source";
    water.resourceType = genesis::world::ResourceType::Water;
    water.capacity = 50;
    water.regenPerStep = 2;
    db.addInteraction(water);

    genesis::world::Interaction foodWorkshop{};
    foodWorkshop.id = 1001;
    foodWorkshop.mapId = 1;
    foodWorkshop.sceneId = 100;
    foodWorkshop.kind = genesis::world::InteractionKind::Resource;
    foodWorkshop.coordLocal = {5, 5};
    foodWorkshop.coordGlobal = foodWorkshop.coordLocal;
    foodWorkshop.name = "food_workshop";
    foodWorkshop.resourceType = genesis::world::ResourceType::Food;
    foodWorkshop.capacity = 40;
    foodWorkshop.regenPerStep = 0;
    foodWorkshop.meta = json{
        {"workshop",
         {{"outputUnits", 3},
          {"initial", 0},
          {"inputs", json::array({json{{"type", "Water"}, {"units", 2}}})}}}
    };
    db.addInteraction(foodWorkshop);

    genesis::agents::ProductionPlanner planner(db);

    genesis::agents::components::AgentLocation2D location{};
    location.mapId = 1;
    location.x = 10.0f;
    location.y = 10.0f;

    genesis::agents::components::CarriedResources carried{};

    genesis::agents::ProductionPlanner::Request request{};
    request.location = &location;
    request.carried = &carried;
    request.targetWorkshop = foodWorkshop.id;
    request.outputType = genesis::world::ResourceType::Food;
    request.outputAmount = 2;
    request.currentRetries = 0;
    request.need = genesis::agents::NeedType::Hunger;
    request.reliefPerUnit = 12.0f;

    const auto planOpt = planner.buildRecoveryPlan(request);
    ASSERT_TRUE(planOpt.has_value());
    const auto& plan = *planOpt;

    ASSERT_GE(plan.size(), 3U);
    EXPECT_EQ(plan.back().type, genesis::agents::ActionType::ConsumeResource);
    EXPECT_EQ(plan.back().interaction, foodWorkshop.id);
    EXPECT_EQ(plan.back().resource, genesis::world::ResourceType::Food);
    EXPECT_EQ(plan.back().retries, 1U);

    bool sawTakeWater = false;
    bool sawProduceFood = false;
    for (const auto& task : plan) {
        if (task.type == genesis::agents::ActionType::TakeResource &&
            task.resource == genesis::world::ResourceType::Water &&
            task.interaction == water.id) {
            sawTakeWater = true;
        }
        if (task.type == genesis::agents::ActionType::ProduceResource &&
            task.resource == genesis::world::ResourceType::Food &&
            task.interaction == foodWorkshop.id) {
            sawProduceFood = true;
        }
    }
    EXPECT_TRUE(sawTakeWater);
    EXPECT_TRUE(sawProduceFood);
}

