#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>

#include "genesis/worldgen/ConfigLoader.hpp"
#include "genesis/worldgen/Generator.hpp"

namespace {

std::filesystem::path repoPath(std::string_view relative) {
    const std::filesystem::path root{GENESIS_TEST_SOURCE_DIR};
    return root / std::filesystem::path(relative);
}

struct TempTomlFile
{
    std::filesystem::path path;

    explicit TempTomlFile(std::string_view content)
    {
        const auto base = std::filesystem::temp_directory_path();
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        path = base / ("genesis_worldgen_generator_test_" + std::to_string(now) + ".toml");

        std::ofstream out(path, std::ios::trunc);
        out << content;
        out.flush();
    }

    ~TempTomlFile()
    {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
};

std::optional<genesis::worldgen::NodePlacement> findPlacement(const genesis::worldgen::LayoutDraft& layout,
                                                              std::size_t localId) {
    for (const auto& p : layout.placements) {
        if (p.local_id == localId) {
            return p;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> parsePortalTarget(const genesis::worldgen::NodeDraft& node) {
    constexpr std::string_view prefix = "portal_target=";
    for (const auto& tag : node.tags) {
        if (tag.size() >= prefix.size() && tag.compare(0, prefix.size(), prefix.data(), prefix.size()) == 0) {
            try {
                return static_cast<std::size_t>(std::stoull(tag.substr(prefix.size())));
            } catch (...) {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

} // namespace

TEST(WorldgenGenerator, ProducesInteractiveNodesAndPlacesNearParent) {
    using namespace genesis::worldgen;

    auto config = load_config(repoPath("data/worldgen/default.toml"));
    auto generated = generate_world(config, Seed{123});

    bool sawResource = false;
    bool sawPortal = false;

    ASSERT_FALSE(generated.topology.nodes.empty());
    ASSERT_EQ(generated.layout.placements.size(), generated.topology.nodes.size());

    for (const auto& node : generated.topology.nodes) {
        if (node.kind == DraftNodeKind::InteractiveResource) {
            sawResource = true;
            bool hasType = false;
            for (const auto& tag : node.tags) {
                if (tag.rfind("resource_type=", 0) == 0) {
                    hasType = true;
                    break;
                }
            }
            EXPECT_TRUE(hasType);
        }
        if (node.kind == DraftNodeKind::InteractivePortal) {
            sawPortal = true;
            EXPECT_TRUE(parsePortalTarget(node).has_value());
        }

        if (node.kind == DraftNodeKind::InteractiveResource || node.kind == DraftNodeKind::InteractivePortal) {
            ASSERT_TRUE(node.parent.has_value());

            const auto childP = findPlacement(generated.layout, node.local_id);
            const auto parentP = findPlacement(generated.layout, *node.parent);
            ASSERT_TRUE(childP.has_value());
            ASSERT_TRUE(parentP.has_value());

            const double dx = std::abs(childP->x - parentP->x);
            const double dy = std::abs(childP->y - parentP->y);
            EXPECT_LE(dx, 6.0);
            EXPECT_LE(dy, 6.0);
        }
    }

    EXPECT_TRUE(sawResource);
    EXPECT_TRUE(sawPortal);
}

TEST(WorldgenGenerator, WeightedResourceTypesCanForceSingleType) {
    using namespace genesis::worldgen;

    TempTomlFile file(R"(
        [world]
        name = "demo"

        [topology.cluster]
        min_clusters = 1
        max_clusters = 1
        min_nodes_per_cluster = 3
        max_nodes_per_cluster = 3

        [topology.corridor]
        enabled = false

        [layout.grid]
        cell_width = 6.0
        cell_height = 5.0
        columns = 3
        margin = 2.0

        [tilemap]
        tile_size = 32
        base_extent = 28

        [worlddb.resources]
        per_map = 2
        types = ["Food", "Water"]
        weights = [1.0, 0.0]
        capacity = 10
        regen_per_step = 1
    )");

    auto config = load_config(file.path);
    auto generated = generate_world(config, Seed{1});

    for (const auto& node : generated.topology.nodes) {
        if (node.kind != DraftNodeKind::InteractiveResource) {
            continue;
        }
        bool isFood = false;
        for (const auto& tag : node.tags) {
            if (tag == "resource_type=Food") {
                isFood = true;
                break;
            }
        }
        EXPECT_TRUE(isFood);
    }
}
