#pragma once

#include <cstdint>

#include "genesis/agents/Affordances.hpp"
#include "genesis/world/WorldTypes.hpp"

namespace genesis::agents::components {

// AgentCommitment：显式“承诺”v0（最小实现）。
// 目标：把“保持目标/避免频繁切换”的特殊逻辑抽象为可中断的承诺对象。
// 约束：v0 只覆盖单 agent 的目标承诺；协商/合同/违约后果属于后续阶段。
struct AgentCommitment {
    std::uint32_t targetInteractionId{0};
    genesis::agents::AffordanceKind kind{genesis::agents::AffordanceKind::ConsumeFromInteraction};
    std::uint32_t targetEntityId{0}; // for SocializeWithAgent
    genesis::world::ResourceType resource{genesis::world::ResourceType::Food}; // for ConsumeFromInteraction

    std::uint64_t startedAtStep{0};
    std::uint64_t holdUntilStep{0}; // before this step, switching away costs more
};

} // namespace genesis::agents::components

