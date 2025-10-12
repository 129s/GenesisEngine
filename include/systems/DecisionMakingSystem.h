#pragma once

#include "components/ActorComponent.h"
#include "components/MaslowNeedsComponent.h"
#include "components/GoalComponent.h"
#include "components/ActionQueueComponent.h"
#include "components/FoodComponent.h"
#include "components/PositionComponent.h"
#include "components/IdentityComponent.h"
#include "components/RelationshipComponent.h"
#include "components/OwnershipComponent.h"
#include <entt/entt.hpp>
#include <iostream>
#include <algorithm>
#include <cmath>

class DecisionMakingSystem {
public:
    static void update(entt::registry& registry);

private:
    static void planGoHomeAndRest(entt::registry& registry, entt::entity actor, const OwnershipComponent& ownership);
    static void planFindFood(entt::registry& registry, entt::entity actor);
    static void planSocialize(entt::registry& registry, entt::entity actor);
};