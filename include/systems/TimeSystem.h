#pragma once

#include "components/WorldTimeComponent.h"
#include <entt/entt.hpp>

class TimeSystem {
public:
    static void update(entt::registry& registry, int ticks = 1);

    // 如果没有找到WorldTime，创建一个
    static void ensureWorldTimeExists(entt::registry& registry);

    // 获取当前时间字符串
    static std::string getCurrentTimeString(const entt::registry& registry);
};