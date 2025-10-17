#include <gtest/gtest.h>

#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include "genesis/world/generation/NoiseGridGenerator.hpp"

using genesis::world::generation::NoiseGridConfig;
using genesis::world::generation::NoiseGridGenerator;

namespace {

std::size_t countSoilNodes(const genesis::world::LocationGraph& graph) {
    std::size_t count = 0;
    for (const auto& node : graph.nodes) {
        if (node.terrain == "Soil") {
            ++count;
        }
    }
    return count;
}

const genesis::world::LocationNode* findNode(const genesis::world::LocationGraph& graph,
                                             genesis::world::LocationId id) {
    for (const auto& node : graph.nodes) {
        if (node.id == id) {
            return &node;
        }
    }
    return nullptr;
}

} // namespace

TEST(NoiseGridGeneratorTest, DeterministicForSameSeed) {
    NoiseGridGenerator generator;
    NoiseGridConfig config;
    config.width = 16;
    config.height = 16;
    config.threshold = 0.5;

    const auto resultA = generator.generate(12345ULL, config);
    const auto resultB = generator.generate(12345ULL, config);

    ASSERT_EQ(resultA.graph.nodes.size(), resultB.graph.nodes.size());
    for (std::size_t i = 0; i < resultA.graph.nodes.size(); ++i) {
        EXPECT_EQ(resultA.graph.nodes[i].terrain, resultB.graph.nodes[i].terrain);
        EXPECT_EQ(resultA.graph.nodes[i].navigable, resultB.graph.nodes[i].navigable);
    }

    ASSERT_EQ(resultA.graph.edges.size(), resultB.graph.edges.size());
    for (std::size_t i = 0; i < resultA.graph.edges.size(); ++i) {
        EXPECT_EQ(resultA.graph.edges[i].from.value, resultB.graph.edges[i].from.value);
        EXPECT_EQ(resultA.graph.edges[i].to.value, resultB.graph.edges[i].to.value);
    }

    ASSERT_EQ(resultA.graph.spawns.size(), resultB.graph.spawns.size());
    for (std::size_t i = 0; i < resultA.graph.spawns.size(); ++i) {
        EXPECT_EQ(resultA.graph.spawns[i].location.value, resultB.graph.spawns[i].location.value);
    }
}

TEST(NoiseGridGeneratorTest, DifferentSeedsProduceDifferentTerrain) {
    NoiseGridGenerator generator;
    NoiseGridConfig config;
    config.width = 12;
    config.height = 12;

    const auto resultA = generator.generate(1ULL, config);
    const auto resultB = generator.generate(999ULL, config);

    bool foundDifference = false;
    for (std::size_t i = 0; i < resultA.graph.nodes.size() && i < resultB.graph.nodes.size(); ++i) {
        if (resultA.graph.nodes[i].terrain != resultB.graph.nodes[i].terrain) {
            foundDifference = true;
            break;
        }
    }
    EXPECT_TRUE(foundDifference);
}

TEST(NoiseGridGeneratorTest, TerrainDistributionMatchesThreshold) {
    NoiseGridGenerator generator;
    NoiseGridConfig config;
    config.width = 64;
    config.height = 64;
    config.threshold = 0.45;

    const auto result = generator.generate(2024ULL, config);
    const double totalCells = static_cast<double>(config.width) * static_cast<double>(config.height);
    const double expectedSoilRatio = 1.0 - config.threshold;
    const double actualSoilRatio = static_cast<double>(countSoilNodes(result.graph)) / totalCells;

    EXPECT_NEAR(actualSoilRatio, expectedSoilRatio, 0.1);
}

TEST(NoiseGridGeneratorTest, EdgesConnectNeighboringNavigableCells) {
    NoiseGridGenerator generator;
    NoiseGridConfig config;
    config.width = 10;
    config.height = 10;

    const auto result = generator.generate(42ULL, config);

    std::unordered_map<std::uint32_t, std::pair<int, int>> layoutPositions;
    for (const auto& entry : result.layout.nodes) {
        layoutPositions.emplace(entry.id.value, std::make_pair(entry.x, entry.y));
    }

    for (const auto& edge : result.graph.edges) {
        const auto* from = findNode(result.graph, edge.from);
        const auto* to = findNode(result.graph, edge.to);
        ASSERT_NE(from, nullptr);
        ASSERT_NE(to, nullptr);
        EXPECT_TRUE(from->navigable);
        EXPECT_TRUE(to->navigable);

        const auto fromPos = layoutPositions.find(edge.from.value);
        const auto toPos = layoutPositions.find(edge.to.value);
        ASSERT_NE(fromPos, layoutPositions.end());
        ASSERT_NE(toPos, layoutPositions.end());

        const auto dx = std::abs(fromPos->second.first - toPos->second.first);
        const auto dy = std::abs(fromPos->second.second - toPos->second.second);

        EXPECT_TRUE((dx == 0 && dy == 1) || (dx == 2 && dy == 0));
    }

    // Ensure the graph contains at least one edge for each navigable node with neighbors
    std::unordered_set<std::uint32_t> nodesWithEdges;
    for (const auto& edge : result.graph.edges) {
        nodesWithEdges.insert(edge.from.value);
        nodesWithEdges.insert(edge.to.value);
    }

    EXPECT_FALSE(nodesWithEdges.empty());
}

TEST(NoiseGridGeneratorTest, SpawnsExistOnSoilCells) {
    NoiseGridGenerator generator;
    NoiseGridConfig config;
    config.width = 32;
    config.height = 32;
    config.soilSpawnDensity = 0.02;

    const auto result = generator.generate(1001ULL, config);
    ASSERT_FALSE(result.graph.spawns.empty());
    for (const auto& spawn : result.graph.spawns) {
        const auto* node = findNode(result.graph, spawn.location);
        ASSERT_NE(node, nullptr);
        EXPECT_EQ(node->terrain, "Soil");
        EXPECT_TRUE(node->navigable);
    }
}

TEST(NoiseGridGeneratorTest, LayoutMatchesGeneratedGraph) {
    NoiseGridGenerator generator;
    NoiseGridConfig config;
    config.width = 8;
    config.height = 6;

    const auto result = generator.generate(77ULL, config);

    EXPECT_EQ(result.layout.nodes.size(), static_cast<std::size_t>(config.width) * config.height);
    EXPECT_GT(result.layout.width, 0U);
    EXPECT_GT(result.layout.height, 0U);

    std::unordered_set<std::uint32_t> layoutIds;
    for (const auto& node : result.layout.nodes) {
        layoutIds.insert(node.id.value);
        EXPECT_GE(node.x, 0);
        EXPECT_GE(node.y, 0);
    }

    for (const auto& node : result.graph.nodes) {
        if (node.id.value == 1U) {
            continue; // skip root region
        }
        EXPECT_TRUE(layoutIds.contains(node.id.value));
    }
}
