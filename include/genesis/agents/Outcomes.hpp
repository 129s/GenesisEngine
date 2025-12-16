#pragma once

#include <cstdint>
#include <vector>

#include "genesis/agents/Needs.hpp"
#include "genesis/world/WorldTypes.hpp"

namespace genesis::agents {

enum class ResourceAttemptFailure : std::uint8_t {
    None = 0,
    Stockout,
    PlanningFailed,
    MissingInteraction,
    Unreachable,
    Other
};

struct ResourceAttemptOutcome {
    std::uint32_t interaction{0};
    genesis::world::ResourceType resourceType{genesis::world::ResourceType::Food};
    NeedType need{NeedType::Hunger};
    std::uint32_t wantedUnits{0};
    std::uint32_t obtainedUnits{0};
    bool recoveryPlanned{false};
    ResourceAttemptFailure failure{ResourceAttemptFailure::None};
};

struct CriticalPreemptOutcome {
    NeedType need{NeedType::Hunger};
};

} // namespace genesis::agents

namespace genesis::agents::components {

struct AgentOutcomeBuffer {
    std::vector<genesis::agents::ResourceAttemptOutcome> resourceAttempts;
    std::vector<genesis::agents::CriticalPreemptOutcome> criticalPreempts;

    void clear() noexcept {
        resourceAttempts.clear();
        criticalPreempts.clear();
    }
};

} // namespace genesis::agents::components
