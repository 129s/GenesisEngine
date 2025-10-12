// 禁用SDL的main重定义
#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <entt/entt.hpp>
#include <iostream>
#include <memory>
#include <chrono>

#include "components/PositionComponent.h"
#include "components/RenderableComponent.h"
#include "components/WorldTimeComponent.h"
#include "components/ActorComponent.h"
#include "components/VelocityComponent.h"
#include "components/MaslowNeedsComponent.h"
#include "components/FoodComponent.h"
#include "components/GoalComponent.h"
#include "components/ActionQueueComponent.h"
#include "components/IdentityComponent.h"
#include "components/KnowledgeComponent.h"
#include "components/RelationshipComponent.h"
#include "components/ZoneComponent.h"
#include "components/OwnershipComponent.h"
#include "systems/RenderSystem.h"
#include "systems/TimeSystem.h"
#include "systems/DebugUiSystem.h"
#include "systems/MovementSystem.h"
#include "systems/WanderAiSystem.h"
#include "systems/NeedsSystem.h"
#include "systems/DecisionMakingSystem.h"
#include "systems/ActionExecutionSystem.h"
#include "systems/EntityPicker.h"
#include "systems/ConfigManager.h"
#include "systems/WorldFactory.h"
#include "systems/InputHandler.h"
#include "systems/Graphics.h"

class Application
{
public:
    Application() : running_(false) {}

    ~Application()
    {
        // Graphics 会自动清理
    }

    bool initialize()
    {
        // 创建配置管理器
        configManager_ = std::make_unique<ConfigManager>();

        // 加载配置文件
        if (!configManager_->loadConfig())
        {
            std::cout << "使用默认配置启动游戏" << std::endl;
        }

        // 输出当前配置（调试用）
        if (DEBUG_CONFIG.enable_console_output)
        {
            CONFIG.printConfig();
        }

        // 设置SDL提示，使用像素风格缩放
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

        // 初始化SDL
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
        {
            std::cerr << "SDL初始化失败: " << SDL_GetError() << std::endl;
            return false;
        }

        // 初始化DebugUi（允许失败，但不阻止程序运行）
        if (configManager_->getDebugConfig().enable_debug_ui && !DebugUiSystem::initialize())
        {
            std::cerr << "警告: DebugUi初始化失败，将使用控制台模式" << std::endl;
        }

        // 创建窗口
        window_ = SDL_CreateWindow(
            DISPLAY_CONFIG.window_title.c_str(),
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            DISPLAY_CONFIG.window_width,
            DISPLAY_CONFIG.window_height,
            SDL_WINDOW_SHOWN);

        if (!window_)
        {
            std::cerr << "窗口创建失败: " << SDL_GetError() << std::endl;
            return false;
        }

        // 创建渲染器
        renderer_ = SDL_CreateRenderer(
            window_,
            -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

        if (!renderer_)
        {
            std::cerr << "渲染器创建失败: " << SDL_GetError() << std::endl;
            return false;
        }

        // 创建游戏纹理（低分辨率画布）
        gameTexture_ = SDL_CreateTexture(
            renderer_,
            SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_TARGET,
            DISPLAY_CONFIG.game_width,
            DISPLAY_CONFIG.game_height);

        if (!gameTexture_)
        {
            std::cerr << "游戏纹理创建失败: " << SDL_GetError() << std::endl;
            return false;
        }

        // 初始化ECS
        registry_ = std::make_unique<entt::registry>();

        // 初始化时间系统
        TimeSystem::ensureWorldTimeExists(*registry_);

        // 创建简单的地图
        RenderSystem::createSimpleMap(*registry_);

        // 创建WorldFactory并加载实体模板
        worldFactory_ = std::make_unique<WorldFactory>(*configManager_);
        if (!worldFactory_->loadEntityTemplates())
        {
            std::cout << "警告: 无法加载实体模板，将使用默认实体创建" << std::endl;
        }

        // 使用WorldFactory创建世界实体
        worldFactory_->createWorld(*registry_);

        running_ = true;
        lastTime_ = std::chrono::high_resolution_clock::now();
        return true;
    }

    void run()
    {
        while (running_)
        {
            // 处理输入事件
            if (!inputHandler_->handleEvents(*registry_, *configManager_))
            {
                running_ = false;
                break;
            }

            update();
            render();
        }
    }

    void handleEvents()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                running_ = false;
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                {
                    running_ = false;
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    // 拾取实体
                    auto pickedEntity = EntityPicker::pickEntity(
                        *registry_,
                        event.button.x,
                        event.button.y,
                        DISPLAY_CONFIG.game_width, // 使用游戏区域宽度而不是窗口宽度
                        DISPLAY_CONFIG.game_height // 使用游戏区域高度而不是窗口高度
                    );
                    DebugUiSystem::setSelectedEntity(pickedEntity);
                }
                break;
            }
        }
    }

    void update()
    {
        // 计算时间增量
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime_).count();
        lastTime_ = currentTime;

        // 更新时间系统
        TimeSystem::update(*registry_, configManager_->getTimeConfig().ticks_per_update);

        // 更新需求系统（每个整点更新）
        NeedsSystem::update(*registry_, *configManager_);

        // 更新决策系统
        DecisionMakingSystem::update(*registry_);

        // 更新行动执行系统
        ActionExecutionSystem::update(*registry_, deltaTime, *configManager_);

        // 对没有行动队列的Actor仍然使用游荡AI
        WanderAiSystem::update(*registry_, deltaTime, *configManager_);

        // 更新移动系统
        MovementSystem::update(*registry_, deltaTime, *configManager_);
    }

    void render()
    {
        // 设置渲染目标到游戏纹理
        SDL_SetRenderTarget(renderer_, gameTexture_);

        // 清空游戏纹理（黑色背景）
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        SDL_RenderClear(renderer_);

        // 渲染游戏内容
        RenderSystem::render(*registry_, renderer_, gameTexture_);

        // 恢复渲染目标到窗口
        SDL_SetRenderTarget(renderer_, nullptr);

        // 清空窗口（黑色背景）
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        SDL_RenderClear(renderer_);

        // 将游戏纹理渲染到右侧区域
        SDL_Rect gameArea = {
            DISPLAY_CONFIG.debug_ui_width, // DebugUI宽度
            0,
            DISPLAY_CONFIG.game_width, // 游戏区域宽度
            DISPLAY_CONFIG.game_height // 游戏区域高度
        };
        SDL_RenderCopy(renderer_, gameTexture_, nullptr, &gameArea);

        // 渲染调试UI（在左侧区域）
        DebugUiSystem::render(*registry_, renderer_);

        // 呈现
        SDL_RenderPresent(renderer_);
    }

    void cleanup()
    {
        if (gameTexture_)
        {
            SDL_DestroyTexture(gameTexture_);
            gameTexture_ = nullptr;
        }

        if (renderer_)
        {
            SDL_DestroyRenderer(renderer_);
            renderer_ = nullptr;
        }

        if (window_)
        {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }

        DebugUiSystem::cleanup();
        SDL_Quit();
    }

private:
    SDL_Window *window_;
    SDL_Renderer *renderer_;
    SDL_Texture *gameTexture_;
    std::unique_ptr<entt::registry> registry_;
    bool running_;
    std::chrono::high_resolution_clock::time_point lastTime_;
};

int main(int argc, char *argv[])
{
    Application app;

    if (!app.initialize())
    {
        std::cerr << "应用程序初始化失败！" << std::endl;
        return -1;
    }

    std::cout << "GenesisEngine 启动成功！" << std::endl;
    std::cout << "按 ESC 键或关闭窗口退出程序。" << std::endl;

    app.run();

    std::cout << "程序正常退出。" << std::endl;
    return 0;
}