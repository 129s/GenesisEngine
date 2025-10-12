#pragma once

#include <SDL2/SDL.h>
#include <entt/entt.hpp>
#include "systems/ConfigManager.h"
#include "systems/EntityPicker.h"
#include "systems/DebugUiSystem.h"

class InputHandler {
public:
    InputHandler() = default;
    ~InputHandler() = default;

    // 处理所有输入事件，返回是否应该继续运行
    bool handleEvents(
        entt::registry& registry,
        const ConfigManager& configManager
    );

    // 检查是否应该退出
    bool shouldQuit() const { return shouldQuit_; }

private:
    bool shouldQuit_ = false;

    // 处理键盘事件
    void handleKeyDown(const SDL_KeyboardEvent& keyEvent);

    // 处理鼠标事件
    void handleMouseButtonDown(
        const SDL_MouseButtonEvent& buttonEvent,
        entt::registry& registry,
        const ConfigManager& configManager
    );

    // 处理退出事件
    void handleQuit();
};