#pragma once

#include <entt/entt.hpp>
#include <random>
#include <iostream>
#include "components/ActorComponent.h"
#include "components/VelocityComponent.h"
#include "components/ActionQueueComponent.h"
#include "systems/ConfigManager.h"

class WanderAiSystem {
public:
    static void update(entt::registry& registry, float deltaTime, const ConfigManager& configManager);

private:
    static void triggerWander(entt::registry& registry, const ConfigManager& configManager);
};