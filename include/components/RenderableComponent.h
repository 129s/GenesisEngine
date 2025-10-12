#pragma once

#include <SDL2/SDL.h>

struct Color {
    Uint8 r, g, b, a;
    Color() : r(255), g(255), b(255), a(255) {}
    Color(Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}
};

struct RenderableComponent {
    Color color;
    bool visible = true;
};