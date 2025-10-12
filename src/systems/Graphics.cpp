#include "systems/Graphics.h"
#include <iostream>

Graphics::~Graphics() {
    cleanup();
}

bool Graphics::initialize(const ConfigManager& configManager) {
    // 设置SDL提示，使用像素风格缩放
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL初始化失败: " << SDL_GetError() << std::endl;
        return false;
    }

    // 创建窗口
    window_ = SDL_CreateWindow(
        configManager.getDisplayConfig().window_title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        configManager.getDisplayConfig().window_width,
        configManager.getDisplayConfig().window_height,
        SDL_WINDOW_SHOWN
    );

    if (!window_) {
        std::cerr << "窗口创建失败: " << SDL_GetError() << std::endl;
        return false;
    }

    // 创建渲染器
    renderer_ = SDL_CreateRenderer(
        window_,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!renderer_) {
        std::cerr << "渲染器创建失败: " << SDL_GetError() << std::endl;
        return false;
    }

    // 创建游戏纹理（低分辨率画布）
    gameTexture_ = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        configManager.getDisplayConfig().game_width,
        configManager.getDisplayConfig().game_height
    );

    if (!gameTexture_) {
        std::cerr << "游戏纹理创建失败: " << SDL_GetError() << std::endl;
        return false;
    }

    return true;
}

void Graphics::render(
    entt::registry& registry,
    const ConfigManager& configManager
) {
    // 设置渲染目标到游戏纹理
    setRenderTargets();

    // 清空游戏纹理（黑色背景）
    clearScreen();

    // 渲染游戏内容
    renderGame(registry);

    // 恢复渲染目标到窗口并清空
    SDL_SetRenderTarget(renderer_, nullptr);
    clearScreen();

    // 设置游戏区域渲染
    setupGameAreaRendering(configManager);

    // 渲染调试UI
    renderDebugUI(registry, configManager);

    // 呈现最终画面
    presentFrame(configManager);
}

void Graphics::cleanup() {
    if (gameTexture_) {
        SDL_DestroyTexture(gameTexture_);
        gameTexture_ = nullptr;
    }

    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }

    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
}

void Graphics::renderGame(entt::registry& registry) {
    RenderSystem::render(registry, renderer_, gameTexture_);
}

void Graphics::renderDebugUI(
    entt::registry& registry,
    const ConfigManager& configManager
) {
    DebugUiSystem::render(registry, renderer_);
}

void Graphics::presentFrame(const ConfigManager& configManager) {
    SDL_RenderPresent(renderer_);
}

void Graphics::setRenderTargets() {
    SDL_SetRenderTarget(renderer_, gameTexture_);
}

void Graphics::clearScreen() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
}

void Graphics::setupGameAreaRendering(const ConfigManager& configManager) {
    // 将游戏纹理渲染到右侧区域
    SDL_Rect gameArea = {
        configManager.getDisplayConfig().debug_ui_width,    // DebugUI宽度
        0,
        configManager.getDisplayConfig().game_width,       // 游戏区域宽度
        configManager.getDisplayConfig().game_height       // 游戏区域高度
    };
    SDL_RenderCopy(renderer_, gameTexture_, nullptr, &gameArea);
}