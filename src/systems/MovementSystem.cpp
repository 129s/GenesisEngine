#include "systems/MovementSystem.h"

void MovementSystem::update(entt::registry& registry, float deltaTime, const ConfigManager& configManager) {
    static float moveAccumulator = 0.0f;
    moveAccumulator += deltaTime;

    // 使用配置中的移动间隔
    float moveInterval = configManager.getAIConfig().movement.move_interval_seconds;
    int tilesPerMove = configManager.getAIConfig().movement.tiles_per_move;

    if (moveAccumulator >= moveInterval) {
        moveAccumulator -= moveInterval;

        // 查询所有拥有PositionComponent和VelocityComponent的实体
        auto view = registry.view<PositionComponent, VelocityComponent>();

        // 计算边界（基于配置中的游戏区域和瓦片尺寸）
        int maxX = configManager.getDisplayConfig().game_width / configManager.getDisplayConfig().tile_size;
        int maxY = configManager.getDisplayConfig().game_height / configManager.getDisplayConfig().tile_size;

        for (auto entity : view) {
            auto& pos = view.get<PositionComponent>(entity);
            auto& vel = view.get<VelocityComponent>(entity);

            // 基于速度更新位置
            pos.x += static_cast<int>(vel.vx) * tilesPerMove;
            pos.y += static_cast<int>(vel.vy) * tilesPerMove;

            // 边界检查
            if (pos.x < 0) pos.x = 0;
            if (pos.x >= maxX) pos.x = maxX - 1;
            if (pos.y < 0) pos.y = 0;
            if (pos.y >= maxY) pos.y = maxY - 1;

            // 如果碰到边界，停止移动
            if (pos.x == 0 || pos.x == maxX - 1) vel.vx = 0;
            if (pos.y == 0 || pos.y == maxY - 1) vel.vy = 0;

            // 静默移动处理（移除冗余输出）
        }
    }
}