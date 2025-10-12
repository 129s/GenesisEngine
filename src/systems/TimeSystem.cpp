#include "systems/TimeSystem.h"

void TimeSystem::update(entt::registry& registry, int ticks) {
    // 查找WorldTime（通常只有一个）
    auto timeView = registry.view<WorldTime>();

    for (auto entity : timeView) {
        auto& worldTime = timeView.get<WorldTime>(entity);
        worldTime.update(ticks);
    }
}

void TimeSystem::ensureWorldTimeExists(entt::registry& registry) {
    auto timeView = registry.view<WorldTime>();
    if (timeView.empty()) {
        auto worldTimeEntity = registry.create();
        registry.emplace<WorldTime>(worldTimeEntity);
    }
}

std::string TimeSystem::getCurrentTimeString(const entt::registry& registry) {
    auto timeView = registry.view<const WorldTime>();
    if (!timeView.empty()) {
        const auto& worldTime = timeView.get<const WorldTime>(timeView.front());
        return worldTime.getTimeString();
    }
    return "No Time System";
}