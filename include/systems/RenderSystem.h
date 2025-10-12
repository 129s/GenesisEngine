#pragma once

#include <entt/entt.hpp>
#include <SDL2/SDL.h>

class RenderSystem {
public:
    static void render(entt::registry& registry, SDL_Renderer* renderer, SDL_Texture* gameTexture);
    static void createSimpleMap(entt::registry& registry);
private:
    static void applyDayNightOverlay(entt::registry& registry, SDL_Renderer* renderer);
};