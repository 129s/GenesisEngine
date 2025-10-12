#include "systems/DecisionMakingSystem.h"
#include "components/MaslowNeedsComponent.h"
#include "components/GoalComponent.h"
#include "components/ActionQueueComponent.h"
#include "components/OwnershipComponent.h"
#include "components/ZoneComponent.h"
#include "components/WorldTimeComponent.h"
#include "components/IdentityComponent.h"
#include <iostream>
#include <cfloat>

void DecisionMakingSystem::update(entt::registry &registry) {
    // 获取世界时间
    auto timeEntities = registry.view<const WorldTimeComponent>();
    if (timeEntities.empty()) {
        return;
    }

    const auto& timeComponent = *timeEntities.begin();
    const auto& worldTime = registry.get<const WorldTimeComponent>(timeComponent).worldTime;
    bool isNight = worldTime.isNight();

    // 处理所有Actor的决策
    auto actors = registry.view<MaslowNeedsComponent, GoalComponent, ActionQueueComponent,
                               OwnershipComponent, const IdentityComponent>();

    for (auto entity : actors) {
        auto& needs = actors.get<MaslowNeedsComponent>(entity);
        auto& goal = actors.get<GoalComponent>(entity);
        auto& actionQueue = actors.get<ActionQueueComponent>(entity);
        const auto& ownership = actors.get<OwnershipComponent>(entity);
        const auto& identity = actors.get<const IdentityComponent>(entity);

        // 如果当前有行动队列，跳过决策
        if (!actionQueue.isEmpty()) {
            continue;
        }

        // 根据主要需求制定目标
        std::string urgentNeed = needs.getMostUrgentNeed();

        if (urgentNeed == "EXHAUSTED" || (urgentNeed == "TIRED" && isNight)) {
            // 夜晚疲劳或极度疲劳时，回家休息
            goal.currentGoal = GoalType::GO_HOME_AND_REST;
            planGoHomeAndRest(registry, entity, ownership);
        } else if (urgentNeed == "EXTREMELY_HUNGRY" || urgentNeed == "HUNGRY") {
            // 寻找食物
            goal.currentGoal = GoalType::FIND_FOOD;
            planFindFood(registry, entity);
        } else if (urgentNeed == "LONELY") {
            // 社交需求 - 去酒馆找人聊天
            goal.currentGoal = GoalType::SOCIALIZE;
            planSocialize(registry, entity);
        } else {
            // 满足状态，可以设置为闲逛或其他活动
            goal.currentGoal = GoalType::WANDER;
            // 不添加行动，让WanderAiSystem处理
        }

        if (!actionQueue.isEmpty()) {
            std::cout << identity.name << " decided to: " << goal.currentGoal << std::endl;
        }
    }
}

void DecisionMakingSystem::planGoHomeAndRest(entt::registry &registry, entt::entity actor,
                                           const OwnershipComponent &ownership) {
    if (!ownership.hasResidence()) {
        std::cout << "Actor has no residence to go home to!" << std::endl;
        return;
    }

    auto& actionQueue = registry.get<ActionQueueComponent>(actor);
    entt::entity homeEntity = ownership.primaryResidence;

    // 添加回家休息的行动序列
    actionQueue.addAction(Action(ActionType::MOVE_TO, homeEntity));
    actionQueue.addAction(Action(ActionType::GO_HOME_AND_REST, homeEntity));
}

void DecisionMakingSystem::planFindFood(entt::registry &registry, entt::entity actor) {
    auto& actionQueue = registry.get<ActionQueueComponent>(actor);

    // 查找最近的食物实体
    auto foods = registry.view<FoodComponent, const PositionComponent>();
    auto actorPos = registry.get<const PositionComponent>(actor);

    entt::entity nearestFood = entt::null;
    float minDistance = FLT_MAX;

    for (auto foodEntity : foods) {
        const auto& foodPos = foods.get<const PositionComponent>(foodEntity);
        float distance = std::sqrt(
            std::pow(actorPos.x - foodPos.x, 2) +
            std::pow(actorPos.y - foodPos.y, 2)
        );

        if (distance < minDistance) {
            minDistance = distance;
            nearestFood = foodEntity;
        }
    }

    if (nearestFood != entt::null) {
        actionQueue.addAction(Action(ActionType::MOVE_TO, nearestFood));
        actionQueue.addAction(Action(ActionType::CONSUME, nearestFood));
    }
}

void DecisionMakingSystem::planSocialize(entt::registry &registry, entt::entity actor) {
    auto& actionQueue = registry.get<ActionQueueComponent>(actor);

    // 查找酒馆区域
    entt::entity tavernEntity = entt::null;
    auto zones = registry.view<ZoneComponent, const PositionComponent>();

    for (auto zoneEntity : zones) {
        const auto& zone = zones.get<ZoneComponent>(zoneEntity);
        if (zone.type == ZoneType::TAVERN) {
            tavernEntity = zoneEntity;
            break;
        }
    }

    if (tavernEntity != entt::null) {
        // 移动到酒馆
        actionQueue.addAction(Action(ActionType::MOVE_TO, tavernEntity));
        // 在酒馆寻找社交目标
        actionQueue.addAction(Action(ActionType::FIND_SOCIAL_TARGET_IN_ZONE, tavernEntity));
        // 聊天（目标待定）
        actionQueue.addAction(Action(ActionType::CHAT_WITH, entt::null));
    }
}