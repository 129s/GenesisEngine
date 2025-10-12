#include "systems/InputHandler.h"

bool InputHandler::handleEvents(
    entt::registry& registry,
    const ConfigManager& configManager
) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                handleQuit();
                break;
            case SDL_KEYDOWN:
                handleKeyDown(event.key);
                break;
            case SDL_MOUSEBUTTONDOWN:
                handleMouseButtonDown(event.button, registry, configManager);
                break;
        }

        // 如果收到退出信号，立即返回
        if (shouldQuit_) {
            return false;
        }
    }

    return true; // 继续运行
}

void InputHandler::handleKeyDown(const SDL_KeyboardEvent& keyEvent) {
    if (keyEvent.keysym.sym == SDLK_ESCAPE) {
        shouldQuit_ = true;
    }
}

void InputHandler::handleMouseButtonDown(
    const SDL_MouseButtonEvent& buttonEvent,
    entt::registry& registry,
    const ConfigManager& configManager
) {
    if (buttonEvent.button == SDL_BUTTON_LEFT) {
        // 拾取实体
        auto pickedEntity = EntityPicker::pickEntity(
            registry,
            buttonEvent.x,
            buttonEvent.y,
            configManager.getDisplayConfig().game_width,    // 使用游戏区域宽度而不是窗口宽度
            configManager.getDisplayConfig().game_height    // 使用游戏区域高度而不是窗口高度
        );
        DebugUiSystem::setSelectedEntity(pickedEntity);
    }
}

void InputHandler::handleQuit() {
    shouldQuit_ = true;
}