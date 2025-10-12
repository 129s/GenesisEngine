#include "systems/EntityPicker.h"

std::pair<int, int> EntityPicker::screenToGame(int screenX, int screenY,
                                               int screenWidth, int screenHeight,
                                               int gameWidth, int gameHeight) {
    float scaleX = static_cast<float>(gameWidth) / screenWidth;
    float scaleY = static_cast<float>(gameHeight) / screenHeight;

    int gameX = static_cast<int>(screenX * scaleX);
    int gameY = static_cast<int>(screenY * scaleY);

    return {gameX, gameY};
}

std::pair<int, int> EntityPicker::gameToTile(int gameX, int gameY, int tileSize) {
    int tileX = gameX / tileSize;
    int tileY = gameY / tileSize;
    return {tileX, tileY};
}

entt::entity EntityPicker::pickEntity(const entt::registry& registry,
                                      int mouseX, int mouseY,
                                      int screenWidth, int screenHeight) {
    // 首先检查鼠标是否在游戏区域内（右侧720像素）
    constexpr int DEBUG_UI_WIDTH = 400;
    constexpr int GAME_AREA_WIDTH = 720;

    if (mouseX < DEBUG_UI_WIDTH) {
        return entt::null; // 鼠标在DebugUI区域，不拾取实体
    }

    // 将鼠标坐标转换为游戏区域内的相对坐标
    int gameAreaMouseX = mouseX - DEBUG_UI_WIDTH;

    // 转换为游戏坐标
    auto [gameX, gameY] = screenToGame(gameAreaMouseX, mouseY, GAME_AREA_WIDTH, screenHeight, 720, 720);
    auto [tileX, tileY] = gameToTile(gameX, gameY, 16);

    // 查找指定位置的实体
    auto view = registry.view<const PositionComponent>();

    for (auto entity : view) {
        const auto& pos = view.get<const PositionComponent>(entity);
        if (pos.x == tileX && pos.y == tileY) {
            return entity;
        }
    }

    return entt::null; // 没有找到实体
}