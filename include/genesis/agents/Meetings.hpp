#pragma once

#include <cstdint>
#include <vector>

#include "genesis/world/WorldDatabase.hpp"

namespace genesis::agents::components {

struct SocialMeetProposal {
    std::uint32_t fromEntityId{0};
    genesis::world::InteractionId meetingInteractionId{0};
    std::uint64_t createdAtStep{0};
    std::uint64_t expireAtStep{0};
};

struct AgentSocialInbox {
    std::vector<SocialMeetProposal> proposals;
};

enum class SocialMeetStatus : std::uint8_t {
    None = 0,
    Proposed,
    Accepted
};

// AgentSocialMeetState：显式“会合承诺”v0（最小实现）。
// 语义：提议->接受后，双方在截止时间前赶赴同一 meetingInteractionId 并等待；
// 若到期仍未会合，记为失败 outcome（供学习系统更新 partner reliability）。
struct AgentSocialMeetState {
    SocialMeetStatus status{SocialMeetStatus::None};
    std::uint32_t partnerEntityId{0};
    genesis::world::InteractionId meetingInteractionId{0};
    std::uint64_t createdAtStep{0};
    std::uint64_t expireAtStep{0};
};

} // namespace genesis::agents::components

