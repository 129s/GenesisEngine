#include "systems/NeedsSystem.h"
#include "systems/ConfigManager.h"
#include "components/MaslowNeedsComponent.h"
#include "components/WorldTimeComponent.h"
#include "components/ActionQueueComponent.h"
#include "components/GoalComponent.h"
#include <iostream>

void NeedsSystem::update(entt::registry &registry, const ConfigManager& configManager) {
    // 获取世界时间
    auto timeEntities = registry.view<const WorldTimeComponent>();
    if (timeEntities.empty()) {
        return;
    }

    const auto& timeComponent = *timeEntities.begin();
    const auto& worldTime = registry.get<const WorldTimeComponent>(timeComponent).worldTime;
    bool isNight = worldTime.isNight();

    // 更新所有Actor的需求
    auto actors = registry.view<MaslowNeedsComponent, const ActionQueueComponent>();

    for (auto entity : actors) {
        auto& needs = actors.get<MaslowNeedsComponent>(entity);
        const auto& actionQueue = actors.get<const ActionQueueComponent>(entity);

        // 判断NPC是否在休息（简单的判断：没有行动或正在执行休息动作）
        bool isResting = false;
        if (!actionQueue.actions.empty()) {
            const auto& currentAction = actionQueue.actions.front();
            if (currentAction.type == ActionType::REST ||
                currentAction.type == ActionType::SLEEP) {
                isResting = true;
            }
        } else if (isNight) {
            // 夜晚没有行动时默认为休息状态
            isResting = true;
        }

        // 更新饥饿度（每小时增加）
        needs.physiological += configManager.getNeedsConfig().physiological.hunger_increase_rate;
        if (needs.physiological > 1.0f) needs.physiological = 1.0f;

        // 更新疲劳度
        if (isResting) {
            // 休息时恢复疲劳
            needs.fatigue -= configManager.getNeedsConfig().fatigue.rest_recovery_rate;
            if (needs.fatigue < 0.0f) needs.fatigue = 0.0f;
        } else {
            // 活动时增加疲劳
            needs.fatigue += configManager.getNeedsConfig().fatigue.activity_increase_rate;
            if (needs.fatigue > 1.0f) needs.fatigue = 1.0f;
        }

        // 夜晚时疲劳度增长更快
        if (isNight && !isResting) {
            needs.fatigue += configManager.getNeedsConfig().fatigue.night_activity_bonus;
        }

        // 社交需求缓慢增长
        needs.loveBelonging += configManager.getNeedsConfig().social.increase_rate;
        if (needs.loveBelonging > 1.0f) needs.loveBelonging = 1.0f;
    }
}

void NeedsSystem::consumeFood(entt::registry& registry, entt::entity consumerEntity, entt::entity foodEntity, const ConfigManager& configManager) {
    // 检查消费者和食物是否存在
    if (!registry.valid(consumerEntity) || !registry.valid(foodEntity)) {
        return;
    }

    // 检查消费者是否有MaslowNeedsComponent
    if (!registry.all_of<MaslowNeedsComponent>(consumerEntity)) {
        return;
    }

    // 检查食物是否有FoodComponent
    if (!registry.all_of<FoodComponent>(foodEntity)) {
        return;
    }

    auto& needs = registry.get<MaslowNeedsComponent>(consumerEntity);
    const auto& food = registry.get<FoodComponent>(foodEntity);

    // 减少饥饿度
    needs.physiological -= food.nutritionValue;
    if (needs.physiological < 0.0f) {
        needs.physiological = 0.0f;
    }

    std::cout << "Entity " << static_cast<uint32_t>(consumerEntity)
              << " consumed food with nutrition " << food.nutritionValue
              << ". New hunger level: " << needs.physiological << std::endl;

    // 销毁食物实体
    registry.destroy(foodEntity);
}