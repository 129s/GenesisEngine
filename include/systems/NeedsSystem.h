#pragma once

#include "components/MaslowNeedsComponent.h"
#include "components/FoodComponent.h"
#include "components/WorldTimeComponent.h"
#include "systems/ConfigManager.h"
#include <entt/entt.hpp>
#include <iostream>

class NeedsSystem {
public:
    // 每小时游戏时间更新一次需求
    static constexpr int UPDATE_INTERVAL_HOURS = 1;

    static void update(entt::registry& registry, const ConfigManager& configManager);

    // 消耗食物来满足需求
    static void consumeFood(entt::registry& registry, entt::entity consumerEntity, entt::entity foodEntity, const ConfigManager& configManager);
};