#include "systems/RenderSystem.h"
#include "components/PositionComponent.h"
#include "components/RenderableComponent.h"
#include "components/ActorComponent.h"
#include "components/WorldTimeComponent.h"
#include "components/ZoneComponent.h"
#include "components/FoodComponent.h"
#include <iostream>

// 游戏常量
constexpr int DEBUG_UI_WIDTH = 400;      // 左侧DebugUI宽度
constexpr int GAME_AREA_WIDTH = 720;     // 游戏区域宽度
constexpr int GAME_HEIGHT = 720;         // 游戏区域高度（正方形）
constexpr int TILE_SIZE = 16;
constexpr int MAP_WIDTH = GAME_AREA_WIDTH / TILE_SIZE;
constexpr int MAP_HEIGHT = GAME_HEIGHT / TILE_SIZE;

void RenderSystem::render(entt::registry &registry, SDL_Renderer *renderer, SDL_Texture * /*gameTexture*/)
{
    // 首先渲染背景tiles（没有ActorComponent、FoodComponent、ZoneComponent的实体）
    {
        auto backgroundEntities = registry.view<const PositionComponent, const RenderableComponent>();
        static int renderCount = 0;

        for (auto entity : backgroundEntities)
        {
            // 跳过Actor实体、Food实体和Zone实体，只渲染纯背景
            if (registry.any_of<ActorComponent>(entity) ||
                registry.any_of<FoodComponent>(entity) ||
                registry.any_of<ZoneComponent>(entity))
            {
                continue;
            }

            const auto &pos = backgroundEntities.get<const PositionComponent>(entity);
            const auto &renderable = backgroundEntities.get<const RenderableComponent>(entity);

            if (renderable.visible)
            {
                SDL_Rect rect = {
                    pos.x * TILE_SIZE,  // 游戏纹理内部不需要偏移
                    pos.y * TILE_SIZE,
                    TILE_SIZE,
                    TILE_SIZE};

                SDL_SetRenderDrawColor(
                    renderer,
                    renderable.color.r,
                    renderable.color.g,
                    renderable.color.b,
                    renderable.color.a);

                SDL_RenderFillRect(renderer, &rect);
            }
        }
        renderCount++;
    }

    // 然后渲染区域（Zone实体）
    {
        auto zoneEntities = registry.view<const PositionComponent, const ZoneComponent>();

        for (auto entity : zoneEntities) {
            const auto &zone = zoneEntities.get<const ZoneComponent>(entity);

            // 设置区域颜色（优化配色方案，更柔和且协调）
            Color zoneColor;
            switch (zone.type) {
                case ZoneType::RESIDENCE:
                    zoneColor = Color(70, 130, 180, 80); // 钢蓝色，更柔和
                    break;
                case ZoneType::TAVERN:
                    zoneColor = Color(205, 133, 63, 80); // 秘鲁色，温暖的棕色
                    break;
                case ZoneType::MARKET:
                    zoneColor = Color(144, 238, 144, 80); // 浅绿色，清新
                    break;
                case ZoneType::WORKPLACE:
                    zoneColor = Color(205, 92, 92, 80); // 印度红，柔和的红色
                    break;
                default:
                    zoneColor = Color(128, 128, 128, 80); // 中性灰
                    break;
            }

            SDL_Rect zoneRect = {
                zone.x * TILE_SIZE,
                zone.y * TILE_SIZE,
                zone.width * TILE_SIZE,
                zone.height * TILE_SIZE
            };

            SDL_SetRenderDrawColor(renderer, zoneColor.r, zoneColor.g, zoneColor.b, zoneColor.a);
            SDL_RenderFillRect(renderer, &zoneRect);
        }
    }

    // 接着渲染食物实体
    {
        auto foodEntities = registry.view<const PositionComponent, const RenderableComponent>();

        for (auto entity : foodEntities)
        {
            // 只渲染有FoodComponent的实体
            if (!registry.any_of<FoodComponent>(entity))
            {
                continue;
            }

            const auto &pos = foodEntities.get<const PositionComponent>(entity);
            const auto &renderable = foodEntities.get<const RenderableComponent>(entity);

            if (renderable.visible)
            {
                SDL_Rect rect = {
                    pos.x * TILE_SIZE,  // 游戏纹理内部不需要偏移
                    pos.y * TILE_SIZE,
                    TILE_SIZE,
                    TILE_SIZE};

                // 获取食物组件并使用其颜色
                const auto& food = registry.get<FoodComponent>(entity);
                auto foodColor = food.getColor();
                Color actualColor = Color(foodColor.r, foodColor.g, foodColor.b, foodColor.a);

                // 渲染食物主体
                SDL_SetRenderDrawColor(renderer, actualColor.r, actualColor.g, actualColor.b, actualColor.a);
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }

    // 然后渲染Actor实体（有ActorComponent的实体）
    {
        auto actorEntities = registry.view<const PositionComponent, const RenderableComponent, const ActorComponent>();
        static int actorRenderCount = 0;

        for (auto entity : actorEntities)
        {
            const auto &pos = actorEntities.get<const PositionComponent>(entity);
            const auto &renderable = actorEntities.get<const RenderableComponent>(entity);

            if (renderable.visible)
            {
                SDL_Rect rect = {
                    pos.x * TILE_SIZE,  // 游戏纹理内部不需要偏移
                    pos.y * TILE_SIZE,
                    TILE_SIZE,
                    TILE_SIZE};

                // 优化Actor颜色，使用更现代且协调的配色
                Color actorColor = renderable.color;
                // 如果是默认的红色(255,0,0)，替换为更现代的颜色
                if (renderable.color.r == 255 && renderable.color.g == 0 && renderable.color.b == 0) {
                    actorColor = Color(255, 99, 71, 255); // 番茄色，更柔和
                }

                SDL_SetRenderDrawColor(
                    renderer,
                    actorColor.r,
                    actorColor.g,
                    actorColor.b,
                    actorColor.a);

                SDL_RenderFillRect(renderer, &rect);

                // 静默渲染（移除冗余输出）
                actorRenderCount++;
            }
        }
    }

    // 最后应用昼夜光照效果
    applyDayNightOverlay(registry, renderer);
}

void RenderSystem::createSimpleMap(entt::registry &registry)
{
    // 创建纯色背景地图（移除网格棋盘）
    Color backgroundColor = Color(25, 25, 35, 255); // 更深的灰蓝色纯色背景

    for (int y = 0; y < MAP_HEIGHT; y++)
    {
        for (int x = 0; x < MAP_WIDTH; x++)
        {
            auto entity = registry.create();

            // 添加位置组件
            registry.emplace<PositionComponent>(entity, x, y);

            // 添加渲染组件（统一背景色）
            registry.emplace<RenderableComponent>(entity, backgroundColor, true);
        }
    }
}

void RenderSystem::applyDayNightOverlay(entt::registry &registry, SDL_Renderer *renderer) {
    // 获取世界时间组件
    auto timeEntities = registry.view<WorldTimeComponent>();
    if (timeEntities.empty()) {
        return; // 没有时间组件，不应用光照效果
    }

    const auto& worldTime = timeEntities.get<WorldTimeComponent>(*timeEntities.begin()).worldTime;

    // 计算当前时间的光照颜色和透明度
    float lightLevel = worldTime.getLightLevel();

    SDL_Color overlayColor;
    if (worldTime.isNight()) {
        // 夜晚：深蓝色调
        overlayColor = {50, 50, 100, static_cast<Uint8>(150 * (1.0f - lightLevel))};
    } else if (worldTime.isEvening()) {
        // 黄昏：橙红色调
        overlayColor = {200, 150, 150, static_cast<Uint8>(60 * (1.0f - lightLevel))};
    } else {
        // 白天：无色罩或非常淡的白色
        overlayColor = {255, 255, 255, static_cast<Uint8>(0)};
    }

    // 创建覆盖整个游戏区域的半透明矩形
    SDL_Rect overlayRect = {
        0,
        0,
        GAME_AREA_WIDTH,
        GAME_HEIGHT
    };

    // 设置混合模式以支持半透明
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, overlayColor.r, overlayColor.g, overlayColor.b, overlayColor.a);
    SDL_RenderFillRect(renderer, &overlayRect);

    // 恢复默认混合模式
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}