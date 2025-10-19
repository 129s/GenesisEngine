#include <algorithm>
#include <chrono>
#include <filesystem>
#include <limits>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "genesis/core/Engine.hpp"
#include "genesis/runtime/Runtime.hpp"

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

TEST(RuntimeTest, GeneratesSnapshotsAfterStepping) {
    genesis::runtime::Runtime runtime({});

    const auto noisePath = locateDataFile("data/world/generated/noise_mvp.json");
    ASSERT_FALSE(noisePath.empty()) << "Unable to locate generated noise world data";

    auto loadResult = runtime.loadWorldFromFile(noisePath);
    ASSERT_TRUE(loadResult.success) << loadResult.error;

    runtime.step(5);

    const auto* snapshot = runtime.latestSnapshot();
    ASSERT_NE(snapshot, nullptr);
    EXPECT_GT(snapshot->version, 0U);
    EXPECT_NE(snapshot->capturedAt, std::chrono::steady_clock::time_point{});
    const auto& tick = snapshot->telemetry;
    EXPECT_GT(tick.step, 0U);
    EXPECT_FALSE(tick.resources.empty());
}

TEST(RuntimeTest, LoadsNoiseWorldViaEnvironmentOverride) {
    const auto noisePath = locateDataFile("data/world/generated/noise_mvp.json");
    ASSERT_FALSE(noisePath.empty()) << "Unable to locate generated noise world data";

    genesis::core::Engine engine;
    const auto loadResult = engine.loadWorldFromFile(noisePath);
    ASSERT_TRUE(loadResult.success) << loadResult.error;
    EXPECT_GT(engine.world().locationCount(), 1000U);
    EXPECT_GT(engine.world().resourceSpawnCount(), 10U);
}

TEST(RuntimeTest, AgentCompletesConsumeCycleOnNoiseWorld) {
    const auto noisePath = locateDataFile("data/world/generated/noise_mvp.json");
    ASSERT_FALSE(noisePath.empty()) << "Unable to locate generated noise world data";

    genesis::core::Engine engine;
    const auto loadResult = engine.loadWorldFromFile(noisePath);
    ASSERT_TRUE(loadResult.success) << loadResult.error;

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
    const auto noisePath = locateDataFile("data/world/generated/noise_mvp.json");
    ASSERT_FALSE(noisePath.empty()) << "Unable to locate generated noise world data";

    genesis::runtime::Runtime runtime({});
    auto loadResult = runtime.loadWorldFromFile(noisePath);
    ASSERT_TRUE(loadResult.success) << loadResult.error;

    const auto tempPath = std::filesystem::temp_directory_path() / "genesis_engine_world_save_test.json";
    auto saveResult = runtime.saveWorldToFile(tempPath);
    ASSERT_TRUE(saveResult.success) << saveResult.error;

    auto reloadResult = runtime.loadWorldFromFile(tempPath);
    ASSERT_TRUE(reloadResult.success) << reloadResult.error;
    EXPECT_GT(runtime.engine().world().locationCount(), 0U);

    std::error_code ec;
    std::filesystem::remove(tempPath, ec);
}
