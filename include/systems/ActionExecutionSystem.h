#pragma once

#include "components/ActionQueueComponent.h"
#include "components/GoalComponent.h"
#include "components/PositionComponent.h"
#include "components/VelocityComponent.h"
#include "components/FoodComponent.h"
#include "components/MaslowNeedsComponent.h"
#include "components/ActorComponent.h"
#include "components/IdentityComponent.h"
#include "components/KnowledgeComponent.h"
#include "components/RelationshipComponent.h"
#include "components/OwnershipComponent.h"
#include "components/ZoneComponent.h"
#include "systems/NeedsSystem.h"
#include "systems/ConfigManager.h"
#include <entt/entt.hpp>
#include <iostream>
#include <cmath>

class ActionExecutionSystem {
public:
    static constexpr float MOVE_SPEED = 2.0f;  // 移动速度
    static constexpr float ARRIVAL_THRESHOLD = 1.5f;  // 到达目标的阈值距离
    static constexpr float REST_RATE = 0.2f;  // 休息时疲劳恢复速度

    static void update(entt::registry& registry, float deltaTime, const ConfigManager& configManager);

private:
    static bool executeMoveTo(entt::registry& registry, entt::entity entity,
                             entt::entity target, const PositionComponent& currentPos);

    static bool executeConsume(entt::registry& registry, entt::entity consumer, entt::entity food, const ConfigManager& configManager);

    static bool executeChatWith(entt::registry& registry, entt::entity actor, entt::entity target);

    static bool executeGoHomeAndRest(entt::registry& registry, entt::entity actor, entt::entity homeTarget);

    static bool executeFindSocialTargetInZone(entt::registry& registry, entt::entity actor, entt::entity zoneEntity);

    static void ensureSocialComponents(entt::registry& registry, entt::entity entity);

    static void exchangeKnowledge(entt::registry& registry, entt::entity actor, entt::entity target);

    static void resetGoal(entt::registry& registry, entt::entity entity);
};