#pragma once

#include <cstdint>

namespace genesis::agents::components {

struct AgentLocation2D {
    std::uint32_t mapId{1};
    float x{0.0f};
    float y{0.0f};
};

struct MovementIntent2D {
    std::uint32_t targetMapId{1};
    float targetX{0.0f};
    float targetY{0.0f};
    float speed{1.0f};
};

struct MovementState2D {
    std::uint32_t currentMapId{1};
    float fromX{0.0f};
    float fromY{0.0f};
    float toX{0.0f};
    float toY{0.0f};
    float remaining{0.0f};
    float segment{0.0f};
};

} // namespace genesis::agents::components

