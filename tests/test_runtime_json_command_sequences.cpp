#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "genesis/runtime/Runtime.hpp"

namespace {

std::filesystem::path repoPath(std::string_view relative) {
    const std::filesystem::path root{GENESIS_TEST_SOURCE_DIR};
    return root / std::filesystem::path(relative);
}

std::filesystem::path makeTempDir(std::string_view name) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto dir = std::filesystem::temp_directory_path() / "GenesisEngineTests" / std::string{name} / std::to_string(stamp);
    std::filesystem::create_directories(dir);
    return dir;
}

const Genesis::Runtime::RuntimeEventReport* findReportById(const Genesis::Runtime::SimulationSnapshot* snapshot, std::uint64_t id) {
    if (!snapshot) {
        return nullptr;
    }
    for (const auto& report : snapshot->events) {
        if (report.id == id) {
            return &report;
        }
    }
    return nullptr;
}

bool snapshotHasLabel(const Genesis::Runtime::SimulationSnapshot* snapshot, std::string_view label) {
    if (!snapshot) {
        return false;
    }
    for (const auto& report : snapshot->events) {
        if (report.label == label) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST(RuntimeJsonCommands, WorldDbGenerateLoadsWorldAndBuildsAtlas) {
    Genesis::Runtime::Runtime runtime;

    const auto outDir = makeTempDir("world_db_generate");
    ASSERT_TRUE(std::filesystem::exists(outDir));

    nlohmann::json cmd = {
        {"action", "world.db.generate"},
        {"configPath", repoPath("data/worldgen/default.toml").string()},
        {"outputFolder", outDir.string()},
        {"seed", 1337}
    };

    std::string error;
    const auto id = runtime.enqueueCommandFromJson(cmd, error);
    ASSERT_TRUE(id.has_value()) << error;

    runtime.step(1);
    const auto* snapshot = runtime.latestSnapshot();
    ASSERT_NE(snapshot, nullptr);

    const auto* report = findReportById(snapshot, *id);
    ASSERT_NE(report, nullptr);
    EXPECT_TRUE(report->success) << report->message;

    EXPECT_EQ(runtime.worldVersion(), 1U);
    const auto atlas = runtime.worldAtlas();
    ASSERT_NE(atlas, nullptr);
    EXPECT_FALSE(atlas->maps.empty());

    EXPECT_TRUE(std::filesystem::exists(outDir / "world.json"));
    (void)std::filesystem::remove_all(outDir);
}

TEST(RuntimeJsonCommandSequences, WaitForSuccessChainsCommandsAcrossSteps) {
    Genesis::Runtime::Runtime runtime;
    const auto outDir = makeTempDir("command_sequence");

    nlohmann::json script = {
        {"name", "world-db-cycle-demo"},
        {"commands", nlohmann::json::array({
            nlohmann::json{
                {"action", "world.db.load"},
                {"label", "seq.load"},
                {"folder", repoPath("data/world_new").string()},
                {"waitForSuccess", true}
            },
            nlohmann::json{
                {"action", "resource.consume"},
                {"label", "seq.consume"},
                {"interactionId", 1000},
                {"amount", 10},
                {"waitForSuccess", true}
            },
            nlohmann::json{
                {"action", "world.db.save"},
                {"label", "seq.save"},
                {"folder", outDir.string()}
            }
        })}
    };

    std::string error;
    ASSERT_TRUE(runtime.enqueueCommandSequenceFromJson(script, error)) << error;

    runtime.step(1);
    const auto* snap1 = runtime.latestSnapshot();
    ASSERT_NE(snap1, nullptr);
    EXPECT_TRUE(snapshotHasLabel(snap1, "seq.load"));
    EXPECT_FALSE(snapshotHasLabel(snap1, "seq.consume"));
    EXPECT_FALSE(snapshotHasLabel(snap1, "seq.save"));

    runtime.step(1);
    const auto* snap2 = runtime.latestSnapshot();
    ASSERT_NE(snap2, nullptr);
    EXPECT_TRUE(snapshotHasLabel(snap2, "seq.consume"));
    EXPECT_FALSE(snapshotHasLabel(snap2, "seq.save"));

    runtime.step(1);
    const auto* snap3 = runtime.latestSnapshot();
    ASSERT_NE(snap3, nullptr);
    EXPECT_TRUE(snapshotHasLabel(snap3, "seq.save"));

    EXPECT_TRUE(std::filesystem::exists(outDir / "world.json"));
    (void)std::filesystem::remove_all(outDir);
}

