#include <gtest/gtest.h>

#include <type_traits>

#include "genesis/agents/Namespace.hpp"
#include "genesis/base/Namespace.hpp"
#include "genesis/core/Namespace.hpp"
#include "genesis/diagnostics/Namespace.hpp"
#include "genesis/messaging/Namespace.hpp"
#include "genesis/simulation/Namespace.hpp"
#include "genesis/telemetry/Namespace.hpp"
#include "genesis/world/Namespace.hpp"
#include "genesis/worldgen/Namespace.hpp"

#include "genesis/agents/Needs.hpp"
#include "genesis/agents/Movement2D.hpp"
#include "genesis/base/Version.hpp"
#include "genesis/messaging/EventBus.hpp"
#include "genesis/telemetry/TelemetryBuffer.hpp"
#include "genesis/world/WorldTypes.hpp"

namespace genesis::tests::namespace_bridges {
namespace WorldgenAlias = Genesis::Worldgen;
} // namespace genesis::tests::namespace_bridges

TEST(NamespaceBridges, ExposeExpectedAliases) {
    static_assert(std::is_same_v<Genesis::Agents::NeedType, genesis::agents::NeedType>);
    static_assert(std::is_same_v<Genesis::Agents::Components::AgentLocation2D, genesis::agents::components::AgentLocation2D>);

    static_assert(std::is_same_v<Genesis::Base::SemVer, genesis::base::SemVer>);
    static_assert(std::is_same_v<Genesis::Messaging::EventBus, genesis::messaging::EventBus>);
    static_assert(std::is_same_v<Genesis::Telemetry::TickTelemetry, genesis::telemetry::TickTelemetry>);
    static_assert(std::is_same_v<Genesis::World::MapId, genesis::world::MapId>);

    EXPECT_TRUE(true);
}
