#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "genesis/runtime/Runtime.hpp"

namespace {

std::filesystem::path repoPath(std::string_view relative) {
    const std::filesystem::path root{GENESIS_TEST_SOURCE_DIR};
    return root / std::filesystem::path(relative);
}

} // namespace

TEST(RuntimeMultiAgentRegression, PlannerSplitsTargetsUnderCrowding) {
    Genesis::Runtime::RuntimeConfig config{};
    config.initialWorldPath = repoPath("data/world_multiagent");

    Genesis::Runtime::Runtime runtime(config);

    runtime.step(1);
    const auto* snapshot0 = runtime.latestSnapshot();
    ASSERT_NE(snapshot0, nullptr);

    for (const auto& agent : snapshot0->telemetry.agents) {
        runtime.deleteAgent(agent.entityId);
    }

    constexpr std::uint32_t kAgentCount = 12;
    std::vector<std::uint32_t> agents;
    agents.reserve(kAgentCount);
    for (std::uint32_t i = 0; i < kAgentCount; ++i) {
        Genesis::Simulation::AgentSpawnParams2D params{};
        params.location.mapId = 1;
        params.location.x = 0.0f;
        params.location.y = 0.0f;
        agents.push_back(runtime.createAgent(params));
    }

    std::unordered_set<std::uint32_t> agentSet;
    agentSet.reserve(agents.size());
    for (const auto id : agents) {
        agentSet.insert(id);
    }

    bool sawSplit = false;
    for (std::uint64_t step = 0; step < 400; ++step) {
        runtime.step(1);
        const auto* snapshot = runtime.latestSnapshot();
        ASSERT_NE(snapshot, nullptr);

        std::unordered_map<std::uint32_t, genesis::world::ResourceType> resourceTypeByInteraction;
        resourceTypeByInteraction.reserve(snapshot->telemetry.resources.size());

        std::uint32_t foodResourceCount = 0;
        std::uint32_t drinkResourceCount = 0;
        for (const auto& resource : snapshot->telemetry.resources) {
            resourceTypeByInteraction[resource.interactionId] = resource.type;
            if (resource.type == genesis::world::ResourceType::Food) {
                ++foodResourceCount;
            } else if (resource.type == genesis::world::ResourceType::Drink) {
                ++drinkResourceCount;
            }
        }

        if (foodResourceCount < 2 && drinkResourceCount < 2) {
            continue;
        }

        std::unordered_set<std::uint32_t> foodTargets;
        std::unordered_set<std::uint32_t> drinkTargets;

        std::uint32_t decisionCount = 0;
        for (const auto& decision : snapshot->telemetry.plannerDecisions) {
            if (!agentSet.contains(decision.entityId)) {
                continue;
            }
            ++decisionCount;
            auto it = resourceTypeByInteraction.find(decision.target);
            if (it == resourceTypeByInteraction.end()) {
                continue;
            }
            if (it->second == genesis::world::ResourceType::Food) {
                foodTargets.insert(decision.target);
            } else if (it->second == genesis::world::ResourceType::Drink) {
                drinkTargets.insert(decision.target);
            }
        }

        if (decisionCount < kAgentCount) {
            continue;
        }

        if ((foodResourceCount >= 2 && foodTargets.size() >= 2) || (drinkResourceCount >= 2 && drinkTargets.size() >= 2)) {
            sawSplit = true;
            break;
        }
    }

    EXPECT_TRUE(sawSplit);
}

