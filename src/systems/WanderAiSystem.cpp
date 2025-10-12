#include "systems/WanderAiSystem.h"

void WanderAiSystem::update(entt::registry& registry, float deltaTime, const ConfigManager& configManager) {
    static float accumulator = 0.0f;
    accumulator += deltaTime;

    // 使用配置中的触发间隔
    float triggerInterval = configManager.getAIConfig().wander.trigger_interval_seconds;
    if (accumulator >= triggerInterval) {
        accumulator = 0.0f;
        if (configManager.getDebugConfig().enable_console_output) {
            std::cout << "--- Wander AI Triggered! ---" << std::endl;
        }
        triggerWander(registry, configManager);
    }
}

void WanderAiSystem::triggerWander(entt::registry& registry, const ConfigManager& configManager) {
    // 查询所有拥有ActorComponent和VelocityComponent但没有行动队列的实体
    auto view = registry.view<ActorComponent, VelocityComponent>();

    static std::random_device rd;
    static std::mt19937 gen(rd());

    // 使用配置中的方向选择范围
    int directionChoices = configManager.getAIConfig().wander.direction_choices;
    static std::uniform_int_distribution<> dis(0, directionChoices - 1);

    float movementSpeed = configManager.getAIConfig().wander.movement_speed;
    float stopProbability = configManager.getAIConfig().wander.stop_probability;

    int actorCount = 0;
    for (auto entity : view) {
        // 跳过有行动队列的实体（它们正在执行智能行为）
        if (registry.all_of<ActionQueueComponent>(entity) &&
            !registry.get<ActionQueueComponent>(entity).isEmpty()) {
            continue;
        }

        actorCount++;
        auto& vel = view.get<VelocityComponent>(entity);

        // 随机选择一个移动方向
        int direction = dis(gen);

        switch (direction) {
            case 0: vel.vx = -movementSpeed; vel.vy = 0.0f; break;        // 左
            case 1: vel.vx = movementSpeed;  vel.vy = 0.0f; break;        // 右
            case 2: vel.vx = 0.0f;           vel.vy = -movementSpeed; break; // 上
            case 3: vel.vx = 0.0f;           vel.vy = movementSpeed; break; // 下
            default: vel.vx = 0.0f;          vel.vy = 0.0f; break;        // 停止
        }

        if (configManager.getDebugConfig().enable_console_output) {
            std::cout << "Wandering Actor " << static_cast<uint32_t>(entity)
                      << " velocity set to (" << vel.vx << "," << vel.vy << ")" << std::endl;
        }
    }

    if (configManager.getDebugConfig().enable_console_output) {
        std::cout << "Total wandering actors processed: " << actorCount << std::endl;
    }
}