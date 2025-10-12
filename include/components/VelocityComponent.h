#pragma once

struct VelocityComponent {
    float vx = 0.0f;
    float vy = 0.0f;

    VelocityComponent() = default;
    VelocityComponent(float x, float y) : vx(x), vy(y) {}
};