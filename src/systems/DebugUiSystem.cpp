#include "systems/DebugUiSystem.h"
#include "systems/TimeSystem.h"
#include "components/ActorComponent.h"
#include "components/PositionComponent.h"
#include "components/VelocityComponent.h"
#include "components/MaslowNeedsComponent.h"
#include "components/GoalComponent.h"
#include "components/ActionQueueComponent.h"
#include "components/IdentityComponent.h"
#include "components/KnowledgeComponent.h"
#include "components/RelationshipComponent.h"
#include "components/ZoneComponent.h"
#include "components/OwnershipComponent.h"
#include "components/WorldTimeComponent.h"
#include <iostream>

entt::entity DebugUiSystem::selectedEntity_ = entt::null;
TTF_Font *DebugUiSystem::font_ = nullptr;
bool DebugUiSystem::initialized_ = false;

bool DebugUiSystem::initialize()
{
    if (initialized_)
    {
        return true;
    }

    // 初始化SDL_ttf
    if (TTF_Init() == -1)
    {
        std::cerr << "Failed to initialize SDL_ttf: " << TTF_GetError() << std::endl;
        return false;
    }

    // 加载字体（尝试适合像素化显示的字体路径）
    const char *fontPaths[] = {
        "C:/Windows/Fonts/simsun.ttc", // 宋体 - 中文字体，像素化效果好
        "C:/Windows/Fonts/msyh.ttc",   // 微软雅黑
        "C:/Windows/Fonts/arial.ttf",  // Arial
    };

    for (const char *path : fontPaths)
    {
        font_ = TTF_OpenFont(path, 18); // 使用较小字体以获得更好的像素化效果
        if (font_)
        {
            std::cout << "Debug UI initialized with font: " << path << std::endl;
            initialized_ = true;
            return true;
        }
    }

    std::cerr << "Failed to load any font for Debug UI" << std::endl;
    TTF_Quit();
    return false;
}

void DebugUiSystem::cleanup()
{
    if (font_)
    {
        TTF_CloseFont(font_);
        font_ = nullptr;
    }
    if (initialized_)
    {
        TTF_Quit();
        initialized_ = false;
    }
}

void DebugUiSystem::setSelectedEntity(entt::entity entity)
{
    selectedEntity_ = entity;
}

entt::entity DebugUiSystem::getSelectedEntity()
{
    return selectedEntity_;
}

void DebugUiSystem::render(const entt::registry &registry, SDL_Renderer *renderer)
{
    if (!initialized_ || !font_ || !renderer)
    {
        return;
    }

    // 获取渲染器信息以确定屏幕尺寸
    int windowWidth, windowHeight;
    SDL_GetRendererOutputSize(renderer, &windowWidth, &windowHeight);

    // 计算安全的UI显示区域（避免超出屏幕）
    int maxUiWidth = windowWidth - 20;   // 左右各留10像素边距
    int maxUiHeight = windowHeight - 20; // 上下各留10像素边距

    // 渲染时间信息（左上角） - 增强版本
    auto timeEntities = registry.view<const WorldTimeComponent>();
    if (!timeEntities.empty())
    {
        const auto &worldTimeComp = *timeEntities.begin();
        const auto &worldTime = registry.get<const WorldTimeComponent>(worldTimeComp).worldTime;

        std::string timeText = "Time: " + worldTime.getTimeString() + " (" + worldTime.getDayPhase() + ")";
        renderText(renderer, timeText, 10, 10, {255, 255, 255, 255});

        // 显示光照等级
        std::string lightText = "Light: " + std::to_string(static_cast<int>(worldTime.getLightLevel() * 100)) + "%";
        renderText(renderer, lightText, 10, 35, {200, 200, 100, 255});
    }
    else
    {
        std::string timeText = "Time: " + TimeSystem::getCurrentTimeString(registry);
        renderText(renderer, timeText, 10, 10, {255, 255, 255, 255});
    }

    // 渲染实体统计信息
    auto totalEntities = registry.storage<entt::entity>()->size();
    auto actorCount = registry.storage<ActorComponent>() ? registry.storage<ActorComponent>()->size() : 0;
    auto zoneCount = registry.storage<ZoneComponent>() ? registry.storage<ZoneComponent>()->size() : 0;
    std::string entityText = "Entities: " + std::to_string(totalEntities) + " | Actors: " + std::to_string(actorCount) + " | Zones: " + std::to_string(zoneCount);
    renderText(renderer, entityText, 10, 60, {200, 200, 200, 255});

    // 如果有选中的实体，显示其信息
    if (selectedEntity_ != entt::null && registry.valid(selectedEntity_))
    {
        renderEntityInfo(registry, renderer, selectedEntity_, 10, 85, maxUiWidth, maxUiHeight);
    }
}

void DebugUiSystem::renderText(SDL_Renderer *renderer, const std::string &text, int x, int y, SDL_Color color)
{
    if (!initialized_ || !font_ || !renderer)
    {
        return;
    }

    // 创建文本表面 - 使用像素化渲染模式，类似CMD终端效果
    SDL_Surface *surface = TTF_RenderUTF8_Solid(font_, text.c_str(), color);
    if (!surface)
    {
        std::cerr << "Failed to create text surface: " << TTF_GetError() << std::endl;
        return;
    }

    // 创建纹理
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture)
    {
        std::cerr << "Failed to create text texture: " << SDL_GetError() << std::endl;
        SDL_FreeSurface(surface);
        return;
    }

    // 直接渲染文字，不显示背景框
    SDL_Rect dstRect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, nullptr, &dstRect);

    // 清理
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void DebugUiSystem::renderEntityInfo(const entt::registry &registry, SDL_Renderer *renderer,
                                     entt::entity entity, int x, int y, int /*maxUiWidth*/, int maxUiHeight)
{
    if (!initialized_ || !font_ || !renderer)
    {
        return;
    }

    // 检查起始位置是否超出屏幕边界
    if (y >= maxUiHeight)
    {
        return;
    }

    // 渲染实体ID
    std::string entityText = "Selected Entity: " + std::to_string(static_cast<uint32_t>(entity));
    renderText(renderer, entityText, x, y, {255, 255, 0, 255});

    int currentY = y + 25;

    // 渲染位置信息
    if (currentY < maxUiHeight && registry.storage<PositionComponent>() && registry.storage<PositionComponent>()->contains(entity))
    {
        const auto &pos = registry.storage<PositionComponent>()->get(entity);
        std::string posText = "Position: (" + std::to_string(static_cast<int>(pos.x)) + ", " +
                              std::to_string(static_cast<int>(pos.y)) + ")";
        renderText(renderer, posText, x, currentY, {0, 255, 0, 255});
        currentY += 25;
    }

    // 渲染速度信息
    if (currentY < maxUiHeight && registry.storage<VelocityComponent>() && registry.storage<VelocityComponent>()->contains(entity))
    {
        const auto &vel = registry.storage<VelocityComponent>()->get(entity);
        std::string velText = "Velocity: (" + std::to_string(static_cast<int>(vel.vx)) + ", " +
                              std::to_string(static_cast<int>(vel.vy)) + ")";
        renderText(renderer, velText, x, currentY, {0, 200, 255, 255});
        currentY += 25;
    }

    // 渲染Actor标记
    if (currentY < maxUiHeight && registry.storage<ActorComponent>() && registry.storage<ActorComponent>()->contains(entity))
    {
        renderText(renderer, "Type: Actor", x, currentY, {255, 128, 0, 255});
        currentY += 25;

        // 显示马斯洛需求组件
        if (currentY < maxUiHeight && registry.storage<MaslowNeedsComponent>() && registry.storage<MaslowNeedsComponent>()->contains(entity))
        {
            const auto &needs = registry.storage<MaslowNeedsComponent>()->get(entity);

            renderText(renderer, "=== Maslow Needs ===", x, currentY, {255, 255, 0, 255});
            currentY += 25;

            // 生理需求进度条
            std::string physText = "Hunger: " + std::to_string(static_cast<int>(needs.physiological * 100)) + "%";
            renderText(renderer, physText, x, currentY, {255, 100, 100, 255});
            currentY += 20;

            // 简单的进度条显示
            int barWidth = 120; // 进一步减小进度条宽度以适应小屏幕
            int barHeight = 8;
            int barX = x;
            int barY = currentY;

            // 背景条
            SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
            SDL_Rect bgBar = {barX, barY, barWidth, barHeight};
            SDL_RenderFillRect(renderer, &bgBar);

            // 进度条填充
            SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255);
            SDL_Rect fillBar = {barX, barY, static_cast<int>(barWidth * needs.physiological), barHeight};
            SDL_RenderFillRect(renderer, &fillBar);

            // 进度条边框
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
            SDL_RenderDrawRect(renderer, &bgBar);

            currentY += 20;

            // 疲劳度进度条
            std::string fatigueText = "Fatigue: " + std::to_string(static_cast<int>(needs.fatigue * 100)) + "%";
            renderText(renderer, fatigueText, x, currentY, {150, 150, 255, 255});
            currentY += 20;

            barY = currentY;
            SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
            bgBar = {barX, barY, barWidth, barHeight};
            SDL_RenderFillRect(renderer, &bgBar);

            SDL_SetRenderDrawColor(renderer, 150, 150, 255, 255);
            fillBar = {barX, barY, static_cast<int>(barWidth * needs.fatigue), barHeight};
            SDL_RenderFillRect(renderer, &fillBar);

            SDL_RenderDrawRect(renderer, &bgBar);

            currentY += 20;

            // 社交需求进度条
            std::string socialText = "Social: " + std::to_string(static_cast<int>(needs.loveBelonging * 100)) + "%";
            renderText(renderer, socialText, x, currentY, {100, 100, 255, 255});
            currentY += 20;

            barY = currentY;
            SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
            bgBar = {barX, barY, barWidth, barHeight};
            SDL_RenderFillRect(renderer, &bgBar);

            SDL_SetRenderDrawColor(renderer, 100, 100, 255, 255);
            fillBar = {barX, barY, static_cast<int>(barWidth * needs.loveBelonging), barHeight};
            SDL_RenderFillRect(renderer, &fillBar);

            // 进度条边框
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
            SDL_RenderDrawRect(renderer, &bgBar);

            currentY += 20;

            // 需求状态
            if (currentY < maxUiHeight)
            {
                std::string needStatus = "Status: " + needs.getMostUrgentNeed();
                SDL_Color statusColor;
                if (needs.needsFood())
                {
                    statusColor = {255, 0, 0, 255};
                }
                else if (needs.needsRest())
                {
                    statusColor = {150, 150, 255, 255};
                }
                else if (needs.needsSocialization())
                {
                    statusColor = {100, 100, 255, 255};
                }
                else
                {
                    statusColor = {0, 255, 0, 255};
                }
                renderText(renderer, needStatus, x, currentY, statusColor);
                currentY += 30;
            }
        }

        // 显示目标组件
        if (currentY < maxUiHeight && registry.storage<GoalComponent>() && registry.storage<GoalComponent>()->contains(entity))
        {
            const auto &goal = registry.storage<GoalComponent>()->get(entity);

            renderText(renderer, "=== Current Goal ===", x, currentY, {255, 255, 0, 255});
            currentY += 25;

            std::string goalText = "Goal: " + goal.description;
            renderText(renderer, goalText, x, currentY, {100, 255, 100, 255});
            currentY += 25;

            if (currentY < maxUiHeight && goal.targetEntity != entt::null)
            {
                std::string targetText = "Target: Entity " + std::to_string(static_cast<uint32_t>(goal.targetEntity));
                renderText(renderer, targetText, x, currentY, {150, 200, 255, 255});
                currentY += 25;
            }
        }

        // 显示行动队列组件
        if (currentY < maxUiHeight && registry.storage<ActionQueueComponent>() && registry.storage<ActionQueueComponent>()->contains(entity))
        {
            const auto &actionQueue = registry.storage<ActionQueueComponent>()->get(entity);

            renderText(renderer, "=== Action Queue ===", x, currentY, {255, 255, 0, 255});
            currentY += 25;

            if (currentY < maxUiHeight && actionQueue.isEmpty())
            {
                renderText(renderer, "Queue: Empty", x, currentY, {150, 150, 150, 255});
                currentY += 25;
            }
            else if (currentY < maxUiHeight)
            {
                renderText(renderer, "Queue Actions:", x, currentY, {200, 200, 255, 255});
                currentY += 25;

                for (size_t i = 0; i < actionQueue.actions.size() && i < 3 && currentY < maxUiHeight; ++i)
                { // 减少显示数量
                    const auto &action = actionQueue.actions[i];
                    std::string actionStr = "  " + std::to_string(i + 1) + ". ";

                    switch (action.type)
                    {
                    case ActionType::MOVE_TO:
                        actionStr += "MOVE_TO";
                        break;
                    case ActionType::CONSUME:
                        actionStr += "CONSUME";
                        break;
                    case ActionType::SLEEP:
                        actionStr += "SLEEP";
                        break;
                    case ActionType::REST:
                        actionStr += "REST";
                        break;
                    case ActionType::GO_HOME_AND_REST:
                        actionStr += "GO_HOME_AND_REST";
                        break;
                    case ActionType::FIND_SOCIAL_TARGET_IN_ZONE:
                        actionStr += "FIND_SOCIAL_TARGET_IN_ZONE";
                        break;
                    case ActionType::INTERACT:
                        actionStr += "INTERACT";
                        break;
                    case ActionType::CHAT_WITH:
                        actionStr += "CHAT_WITH";
                        break;
                    default:
                        actionStr += "UNKNOWN";
                        break;
                    }

                    if (action.targetEntity != entt::null)
                    {
                        actionStr += " (Entity " + std::to_string(static_cast<uint32_t>(action.targetEntity)) + ")";
                    }

                    SDL_Color actionColor = (i == 0) ? SDL_Color{255, 200, 100, 255} : SDL_Color{180, 180, 180, 255};
                    renderText(renderer, actionStr, x, currentY, actionColor);
                    currentY += 20;
                }

                if (currentY < maxUiHeight && actionQueue.actions.size() > 3)
                {
                    std::string moreText = "  ... and " + std::to_string(actionQueue.actions.size() - 3) + " more";
                    renderText(renderer, moreText, x, currentY, {150, 150, 150, 255});
                    currentY += 20;
                }
            }
        }

        // 显示身份组件（简化版）
        if (currentY < maxUiHeight && registry.storage<IdentityComponent>() && registry.storage<IdentityComponent>()->contains(entity))
        {
            const auto &identity = registry.storage<IdentityComponent>()->get(entity);

            renderText(renderer, "=== Identity ===", x, currentY, {255, 255, 0, 255});
            currentY += 25;

            std::string nameText = "Name: " + identity.getFullName();
            renderText(renderer, nameText, x, currentY, {200, 255, 200, 255});
            currentY += 25;
        }

        // 显示关系组件（简化版）
        if (currentY < maxUiHeight && registry.storage<RelationshipComponent>() && registry.storage<RelationshipComponent>()->contains(entity))
        {
            const auto &relationships = registry.storage<RelationshipComponent>()->get(entity);

            renderText(renderer, "=== Relationships ===", x, currentY, {255, 255, 0, 255});
            currentY += 25;

            std::string relCountText = "Relations: " + std::to_string(relationships.size());
            renderText(renderer, relCountText, x, currentY, {200, 200, 255, 255});
            currentY += 25;
        }

        // 显示知识组件（简化版）
        if (currentY < maxUiHeight && registry.storage<KnowledgeComponent>() && registry.storage<KnowledgeComponent>()->contains(entity))
        {
            const auto &knowledge = registry.storage<KnowledgeComponent>()->get(entity);

            renderText(renderer, "=== Knowledge ===", x, currentY, {255, 255, 0, 255});
            currentY += 25;

            std::string knowledgeCountText = "Facts: " + std::to_string(knowledge.size());
            renderText(renderer, knowledgeCountText, x, currentY, {200, 200, 255, 255});
            currentY += 25;
        }

        // 显示所有权组件
        if (currentY < maxUiHeight && registry.storage<OwnershipComponent>() && registry.storage<OwnershipComponent>()->contains(entity))
        {
            const auto &ownership = registry.storage<OwnershipComponent>()->get(entity);

            renderText(renderer, "=== Ownership ===", x, currentY, {255, 255, 0, 255});
            currentY += 25;

            std::string ownedCountText = "Owned Entities: " + std::to_string(ownership.getOwnedCount());
            renderText(renderer, ownedCountText, x, currentY, {200, 200, 255, 255});
            currentY += 25;

            if (currentY < maxUiHeight && ownership.hasResidence())
            {
                std::string residenceText = "Has Residence: Yes";
                renderText(renderer, residenceText, x, currentY, {100, 255, 100, 255});
                currentY += 25;
            }
            else
            {
                std::string residenceText = "Has Residence: No";
                renderText(renderer, residenceText, x, currentY, {255, 100, 100, 255});
                currentY += 25;
            }
        }

        // 显示区域组件
        if (currentY < maxUiHeight && registry.storage<ZoneComponent>() && registry.storage<ZoneComponent>()->contains(entity))
        {
            const auto &zone = registry.storage<ZoneComponent>()->get(entity);

            renderText(renderer, "=== Zone Info ===", x, currentY, {255, 255, 0, 255});
            currentY += 25;

            std::string zoneTypeText = "Type: " + zone.getTypeName();
            renderText(renderer, zoneTypeText, x, currentY, {200, 200, 255, 255});
            currentY += 25;

            std::string zoneNameText = "Name: " + zone.name;
            renderText(renderer, zoneNameText, x, currentY, {200, 255, 200, 255});
            currentY += 25;

            std::string zoneAreaText = "Area: " + std::to_string(zone.x) + "," + std::to_string(zone.y) +
                                       " [" + std::to_string(zone.width) + "x" + std::to_string(zone.height) + "]";
            renderText(renderer, zoneAreaText, x, currentY, {180, 180, 200, 255});
            currentY += 25;

            if (zone.owner != entt::null)
            {
                std::string ownerText = "Owner: Entity " + std::to_string(static_cast<uint32_t>(zone.owner));
                renderText(renderer, ownerText, x, currentY, {255, 200, 100, 255});
                currentY += 25;
            }
        }
    }
}