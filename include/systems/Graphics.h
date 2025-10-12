#pragma once

#include <SDL2/SDL.h>
#include <entt/entt.hpp>
#include <memory>
#include "systems/ConfigManager.h"
#include "systems/RenderSystem.h"
#include "systems/DebugUiSystem.h"

class Graphics {
public:
    Graphics() : window_(nullptr), renderer_(nullptr), gameTexture_(nullptr) {}
    ~Graphics();

    // 初始化图形系统
    bool initialize(const ConfigManager& configManager);

    // 渲染一帧
    void render(
        entt::registry& registry,
        const ConfigManager& configManager
    );

    // 获取原生渲染器（用于系统兼容）
    SDL_Renderer* getRenderer() const { return renderer_; }

    // 清理资源
    void cleanup();

private:
    // SDL 核心组件
    SDL_Window* window_;
    SDL_Renderer* renderer_;
    SDL_Texture* gameTexture_;

    // 内部渲染方法
    void renderGame(entt::registry& registry);
    void renderDebugUI(entt::registry& registry, const ConfigManager& configManager);
    void presentFrame(const ConfigManager& configManager);

    // 设置渲染状态
    void setRenderTargets();
    void clearScreen();
    void setupGameAreaRendering(const ConfigManager& configManager);
};