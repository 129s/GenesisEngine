#pragma once
#include <string>
#include <entt/entt.hpp>

// 目标类型枚举
enum class GoalType {
    NONE,
    FIND_FOOD,
    FIND_SHELTER,
    SOCIALIZE,
    REST,
    GO_HOME_AND_REST,
    WANDER
};

// 目标组件
struct GoalComponent {
    GoalType currentGoal = GoalType::NONE;
    std::string description = "No goal";
    entt::entity targetEntity = entt::null;  // 目标实体（如食物实体）

    GoalComponent() = default;
    GoalComponent(GoalType goal, const std::string& desc, entt::entity target = entt::null)
        : currentGoal(goal), description(desc), targetEntity(target) {}
};

// GoalType输出运算符
inline std::ostream& operator<<(std::ostream& os, const GoalType& goal) {
    switch (goal) {
        case GoalType::NONE: return os << "NONE";
        case GoalType::FIND_FOOD: return os << "FIND_FOOD";
        case GoalType::FIND_SHELTER: return os << "FIND_SHELTER";
        case GoalType::SOCIALIZE: return os << "SOCIALIZE";
        case GoalType::REST: return os << "REST";
        case GoalType::GO_HOME_AND_REST: return os << "GO_HOME_AND_REST";
        case GoalType::WANDER: return os << "WANDER";
        default: return os << "UNKNOWN";
    }
}