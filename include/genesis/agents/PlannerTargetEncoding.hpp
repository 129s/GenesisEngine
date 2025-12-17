#pragma once

#include <cstdint>

namespace genesis::agents {

// PlannerDecision.target 编码约定：
// - 真实世界交互点：InteractionId（最高位为 0）
// - 代理实体目标（例如社交伙伴）：最高位为 1，低 31 位存 entityId
inline constexpr std::uint32_t kAgentPlannerTargetMask = 0x80000000u;

[[nodiscard]] inline bool isAgentPlannerTarget(std::uint32_t target) noexcept {
    return (target & kAgentPlannerTargetMask) != 0U;
}

[[nodiscard]] inline std::uint32_t encodeAgentPlannerTargetEntityId(std::uint32_t entityId) noexcept {
    return kAgentPlannerTargetMask | (entityId & ~kAgentPlannerTargetMask);
}

[[nodiscard]] inline std::uint32_t decodeAgentPlannerTargetEntityId(std::uint32_t target) noexcept {
    return (target & ~kAgentPlannerTargetMask);
}

} // namespace genesis::agents

