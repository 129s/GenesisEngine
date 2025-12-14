#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string_view>

#include "genesis/runtime/Runtime.hpp"

namespace {

std::filesystem::path repoPath(std::string_view relative) {
    const std::filesystem::path root{GENESIS_TEST_SOURCE_DIR};
    return root / std::filesystem::path(relative);
}

std::optional<genesis::telemetry::ResourceSnapshot> findResource(const genesis::telemetry::TickTelemetry& telemetry,
                                                                 std::uint32_t interactionId) {
    for (const auto& resource : telemetry.resources) {
        if (resource.interactionId == interactionId) {
            return resource;
        }
    }
    return std::nullopt;
}

} // namespace

TEST(RuntimeE2ETest, LoadsWorldMovesAgentAndResourceRegenerates) {
    Genesis::Runtime::RuntimeConfig config{};
    config.initialWorldPath = repoPath("data/world_new");

    Genesis::Runtime::Runtime runtime(config);

    auto db = runtime.worldDatabase();
    ASSERT_NE(db, nullptr);
    EXPECT_EQ(db->maps().size(), 2U);

    runtime.step(1);
    const auto* snapshot1 = runtime.latestSnapshot();
    ASSERT_NE(snapshot1, nullptr);
    ASSERT_FALSE(snapshot1->telemetry.agents.empty());

    const auto& agent1 = snapshot1->telemetry.agents.front();
    EXPECT_EQ(agent1.mapId, 1U);
    EXPECT_NEAR(agent1.position.x, 0.8944f, 1e-3f);
    EXPECT_NEAR(agent1.position.y, 0.4472f, 1e-3f);

    const auto resource1 = findResource(snapshot1->telemetry, 1000U);
    ASSERT_TRUE(resource1.has_value());
    EXPECT_EQ(resource1->capacity, 50U);
    EXPECT_EQ(resource1->current, 50U);

    EXPECT_EQ(runtime.consumeResource(1000U, 10U), 10U);

    runtime.step(1);
    const auto* snapshot2 = runtime.latestSnapshot();
    ASSERT_NE(snapshot2, nullptr);
    const auto resource2 = findResource(snapshot2->telemetry, 1000U);
    ASSERT_TRUE(resource2.has_value());
    EXPECT_EQ(resource2->capacity, 50U);
    EXPECT_EQ(resource2->current, 42U);

    runtime.step(1);
    const auto* snapshot3 = runtime.latestSnapshot();
    ASSERT_NE(snapshot3, nullptr);
    const auto resource3 = findResource(snapshot3->telemetry, 1000U);
    ASSERT_TRUE(resource3.has_value());
    EXPECT_EQ(resource3->capacity, 50U);
    EXPECT_EQ(resource3->current, 44U);
}
