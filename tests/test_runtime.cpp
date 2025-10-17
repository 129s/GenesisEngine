#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <string>

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

class EnvironmentGuard {
public:
    explicit EnvironmentGuard(std::string name)
        : m_name(std::move(name)) {
        if (const char* value = std::getenv(m_name.c_str())) {
            m_previous = value;
            m_hadPrevious = true;
        }
    }

    void set(const std::string& value) {
#ifdef _WIN32
        _putenv_s(m_name.c_str(), value.c_str());
#else
        ::setenv(m_name.c_str(), value.c_str(), 1);
#endif
    }

    ~EnvironmentGuard() {
        if (m_hadPrevious) {
#ifdef _WIN32
            _putenv_s(m_name.c_str(), m_previous.c_str());
#else
            ::setenv(m_name.c_str(), m_previous.c_str(), 1);
#endif
        } else {
#ifdef _WIN32
            _putenv_s(m_name.c_str(), "");
#else
            ::unsetenv(m_name.c_str());
#endif
        }
    }

private:
    std::string m_name;
    std::string m_previous;
    bool m_hadPrevious{false};
};

} // namespace

TEST(RuntimeTest, GeneratesSnapshotsAfterStepping) {
    genesis::runtime::Runtime runtime({});
    runtime.step(5);

    const auto* snapshot = runtime.latestSnapshot();
    ASSERT_NE(snapshot, nullptr);
    EXPECT_GT(snapshot->step, 0U);
    EXPECT_FALSE(snapshot->resources.empty());
}

TEST(RuntimeTest, LoadsNoiseWorldViaEnvironmentOverride) {
    const auto noisePath = locateDataFile("data/world/generated/noise_mvp.json");
    ASSERT_FALSE(noisePath.empty()) << "Unable to locate generated noise world data";

    EnvironmentGuard guard("GENESIS_WORLD_PATH");
    guard.set(noisePath.string());

    genesis::core::Engine engine;
    EXPECT_GT(engine.world().locationCount(), 1000U);
    EXPECT_GT(engine.world().resourceSpawnCount(), 10U);
}

TEST(RuntimeTest, AgentCompletesConsumeCycleOnNoiseWorld) {
    const auto noisePath = locateDataFile("data/world/generated/noise_mvp.json");
    ASSERT_FALSE(noisePath.empty()) << "Unable to locate generated noise world data";

    EnvironmentGuard guard("GENESIS_WORLD_PATH");
    guard.set(noisePath.string());

    genesis::core::Engine engine;

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
