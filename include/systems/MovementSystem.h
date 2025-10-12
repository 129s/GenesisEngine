#pragma once

#include <entt/entt.hpp>
#include <iostream>
#include "components/PositionComponent.h"
#include "components/VelocityComponent.h"
#include "systems/ConfigManager.h"

class MovementSystem {
public:
    static void update(entt::registry& registry, float deltaTime, const ConfigManager& configManager);
};