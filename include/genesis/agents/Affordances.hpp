#pragma once

#include <cstdint>

#include "genesis/agents/Needs.hpp"
#include "genesis/world/WorldTypes.hpp"

namespace genesis::agents {

// Affordance：由“局部可感知世界状态 + 规则”生成的行动可能性（行动原材料）。
// 约束：这是运行时的最小数据结构，不携带叙事语义，不内置剧情模板。
enum class AffordanceKind : std::uint8_t {
    ConsumeFromInteraction,
    SocializeWithAgent,
    ProposeMeetWithAgent,
    AttendMeeting,
    RejectMeeting,
};

struct Affordance {
    AffordanceKind kind{AffordanceKind::ConsumeFromInteraction};

    // Primary drive / main dimension this affordance is expected to affect.
    NeedType primaryNeed{NeedType::Hunger};

    // Targeting.
    std::uint32_t interactionId{0}; // WorldDatabase InteractionId (or encoded agent target for SocializeWithAgent)
    std::uint32_t targetEntityId{0}; // for SocializeWithAgent
    world::ResourceType resource{world::ResourceType::Food}; // for ConsumeFromInteraction

    // Execution parameters.
    std::uint32_t units{0};
    float reliefPerUnit{0.0f};

    // Selection diagnostics (not stable API).
    float travelCost{0.0f};
    float score{0.0f};
};

} // namespace genesis::agents
