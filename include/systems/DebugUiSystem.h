#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <entt/entt.hpp>

class DebugUiSystem {
public:
    static bool initialize();
    static void cleanup();
    static void render(const entt::registry& registry, SDL_Renderer* renderer);

    static void setSelectedEntity(entt::entity entity);
    static entt::entity getSelectedEntity();

private:
    static entt::entity selectedEntity_;
    static TTF_Font* font_;
    static bool initialized_;

    static void renderText(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color = {255, 255, 255, 255});
    static void renderEntityInfo(const entt::registry& registry, SDL_Renderer* renderer,
                                entt::entity entity, int x, int y, int maxUiWidth, int maxUiHeight);
};