#include <gtest/gtest.h>

#include <type_traits>

#include "genesis/NamespaceAliases.hpp"
#include "genesis/agents/Needs.hpp"
#include "genesis/telemetry/TelemetryBuffer.hpp"
#include "genesis/world/WorldTypes.hpp"

TEST(NamespaceAliases, ExposesGenesisAliases) {
    static_assert(std::is_same_v<Genesis::Agents::NeedType, genesis::agents::NeedType>);
    static_assert(std::is_same_v<Genesis::World::MapId, genesis::world::MapId>);
    static_assert(std::is_same_v<Genesis::Telemetry::TickTelemetry, genesis::telemetry::TickTelemetry>);
    EXPECT_TRUE(true);
}

