#pragma once

#include <entt/entt.hpp>
#include "components/PositionComponent.h"

class EntityPicker {
public:
    // 将屏幕坐标转换为游戏坐标
    static std::pair<int, int> screenToGame(int screenX, int screenY,
                                           int screenWidth, int screenHeight,
                                           int gameWidth, int gameHeight);

    // 将游戏坐标转换为Tile坐标
    static std::pair<int, int> gameToTile(int gameX, int gameY, int tileSize = 16);

    // 根据鼠标位置查找实体
    static entt::entity pickEntity(const entt::registry& registry,
                                  int mouseX, int mouseY,
                                  int screenWidth, int screenHeight);
};