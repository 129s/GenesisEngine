#pragma once

#include <cstdint>
#include <optional>

#include "genesis/agents/Personality.hpp"

namespace genesis::simulation {

struct AgentPose2D {
    std::uint32_t mapId{0};
    float x{0.0f};
    float y{0.0f};
};

struct MovementCommand2D {
    std::uint32_t targetMapId{0};
    float targetX{0.0f};
    float targetY{0.0f};
    float speed{0.0f};
};

struct AgentSpawnParams2D {
    AgentPose2D location{};
    std::optional<MovementCommand2D> initialMovement{};
    std::optional<genesis::agents::AgentPersonalityBig5> personality{};
};

} // namespace genesis::simulation
