#include <algorithm>
#include <chrono>
#include <filesystem>
#include <limits>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "genesis/core/Engine.hpp"
#include "genesis/runtime/Runtime.hpp"
#include "genesis/world/WorldLoader.hpp"
#include "genesis/worldgen/ConfigLoader.hpp"
#include "genesis/worldgen/Generator.hpp"

namespace {

std::filesystem::path locateDataFile(const std::filesystem::path& relative) {
    auto current = std::filesystem::current_path();
    for (int depth = 0; depth < 6; ++depth) {
        const auto candidate = current / relative;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }
    return {};
}

} // namespace

genesis::world::LocationGraph generateWorldGraphFromDefaultConfig() {
    const auto configPath = locateDataFile("data/worldgen/default.toml");
    if (configPath.empty()) {
        ADD_FAILURE() << "Unable to locate worldgen config: data/worldgen/default.toml";
        return {};
    }

    auto config = genesis::worldgen::load_config(configPath);
    auto generated = genesis::worldgen::generate_world(config, genesis::worldgen::Seed{1337});
    auto graph = std::move(generated.world_graph);
    if (graph.spawns.empty() && !graph.nodes.empty()) {
        genesis::world::ResourceSpawn spawn{};
        spawn.name = "TestFood";
        spawn.type = genesis::world::ResourceType::Food;
        spawn.location = graph.nodes.front().id;
        spawn.capacity = 32;
        spawn.ratePerStep = 4;
        spawn.local_coord = std::make_pair(0, 0);
        graph.spawns.push_back(std::move(spawn));
    }
    return graph;
}

std::filesystem::path writeWorldGraphToTemp(const genesis::world::LocationGraph& graph) {
    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() /
        ("genesis_world_test_" + std::to_string(timestamp) + ".json");
    auto saveResult = genesis::world::saveWorldToFile(path, graph);
    if (!saveResult.success) {
        ADD_FAILURE() << "Failed to save world graph: " << saveResult.error;
        return {};
    }
    return path;
}

TEST(RuntimeTest, GeneratesSnapshotsAfterStepping) {
    genesis::runtime::Runtime runtime({});

    runtime.engine().reloadWorld(generateWorldGraphFromDefaultConfig());

    runtime.step(5);

    const auto* snapshot = runtime.latestSnapshot();
    ASSERT_NE(snapshot, nullptr);
    EXPECT_GT(snapshot->version, 0U);
    EXPECT_NE(snapshot->capturedAt, std::chrono::steady_clock::time_point{});
    const auto& tick = snapshot->telemetry;
    EXPECT_GT(tick.step, 0U);
    EXPECT_FALSE(tick.resources.empty());
}

TEST(RuntimeTest, LoadsGeneratedWorldFromFile) {
    const auto worldGraph = generateWorldGraphFromDefaultConfig();
    const auto worldPath = writeWorldGraphToTemp(worldGraph);
    ASSERT_FALSE(worldPath.empty());

    genesis::core::Engine engine;
    const auto loadResult = engine.loadWorldFromFile(worldPath);
    ASSERT_TRUE(loadResult.success) << loadResult.error;
    EXPECT_GT(engine.world().locationCount(), 0U);
    EXPECT_GT(engine.world().resourceSpawnCount(), 0U);

    std::error_code ec;
    std::filesystem::remove(worldPath, ec);
}

TEST(RuntimeTest, AgentCompletesConsumeCycleOnGeneratedWorld) {
    genesis::core::Engine engine;
    engine.reloadWorld(generateWorldGraphFromDefaultConfig());

    bool sawPlannerDecision = false;
    bool sawResourceDip = false;
    bool hungerReduced = false;
    float maxHunger = std::numeric_limits<float>::lowest();

    constexpr int kMaxSteps = 6000;
    for (int step = 0; step < kMaxSteps; ++step) {
        engine.step(1);
        const auto& entries = engine.telemetry().entries();
        if (entries.empty()) {
            continue;
        }

        const auto& tick = entries.back();
        if (!tick.plannerDecisions.empty()) {
            sawPlannerDecision = true;
        }

        for (const auto& need : tick.needs) {
            if (need.needName != "Hunger") {
                continue;
            }
            if (need.value > maxHunger) {
                maxHunger = need.value;
            }
            if (maxHunger - need.value > 5.0f) {
                hungerReduced = true;
            }
        }

        for (const auto& resource : tick.resources) {
            if (resource.current < resource.capacity) {
                sawResourceDip = true;
                break;
            }
        }

        if (sawPlannerDecision && sawResourceDip && hungerReduced) {
            break;
        }
    }

    EXPECT_TRUE(sawPlannerDecision);
    EXPECT_TRUE(sawResourceDip);
    EXPECT_TRUE(hungerReduced);
}

TEST(RuntimeTest, SaveAndReloadWorld) {
    genesis::runtime::Runtime runtime({});
    runtime.engine().reloadWorld(generateWorldGraphFromDefaultConfig());

    const auto tempPath = std::filesystem::temp_directory_path() / "genesis_engine_world_save_test.json";
    auto saveResult = runtime.saveWorldToFile(tempPath);
    ASSERT_TRUE(saveResult.success) << saveResult.error;

    auto reloadResult = runtime.loadWorldFromFile(tempPath);
    ASSERT_TRUE(reloadResult.success) << reloadResult.error;
    EXPECT_GT(runtime.engine().world().locationCount(), 0U);

    std::error_code ec;
    std::filesystem::remove(tempPath, ec);
}
