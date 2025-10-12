#pragma once
#include <vector>
#include <entt/entt.hpp>

// 行动类型枚举
enum class ActionType {
    MOVE_TO,
    CONSUME,
    SLEEP,
    REST,
    INTERACT,
    CHAT_WITH,
    GO_HOME_AND_REST,
    FIND_SOCIAL_TARGET_IN_ZONE
};

// 行动结构体
struct Action {
    ActionType type;
    entt::entity targetEntity = entt::null;

    Action() = default;
    Action(ActionType t, entt::entity target = entt::null)
        : type(t), targetEntity(target) {}
};

// 行动队列组件
struct ActionQueueComponent {
    std::vector<Action> actions;

    ActionQueueComponent() = default;

    // 添加行动到队列末尾
    void addAction(const Action& action);

    // 添加行动到队列开头（高优先级）
    void addPriorityAction(const Action& action);

    // 获取下一个行动
    Action getNextAction() const;

    // 完成当前行动，将其从队列中移除
    void completeCurrentAction();

    // 检查队列是否为空
    bool isEmpty() const;

    // 清空队列
    void clear();
};