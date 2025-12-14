#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <optional>
#include <string_view>
#include <thread>

#include <nlohmann/json.hpp>

#include "sandbox/gui/RuntimeBridge.hpp"

namespace {

std::filesystem::path repoPath(std::string_view relative) {
    const std::filesystem::path root{GENESIS_TEST_SOURCE_DIR};
    return root / std::filesystem::path(relative);
}

std::filesystem::path uniqueTempFolder(std::string_view prefix) {
    const auto base = std::filesystem::temp_directory_path();
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return base / (std::string(prefix) + "-" + std::to_string(now));
}

std::optional<genesis::sandbox::gui::RuntimeBridge::CommandProgress> findCommand(
    const std::vector<genesis::sandbox::gui::RuntimeBridge::CommandProgress>& commands,
    const std::string& label) {
    for (const auto& entry : commands) {
        if (entry.label == label) {
            return entry;
        }
    }
    return std::nullopt;
}

std::string dumpCommands(const std::vector<genesis::sandbox::gui::RuntimeBridge::CommandProgress>& commands) {
    using genesis::sandbox::gui::RuntimeBridge;
    std::string out;
    out.reserve(512);
    for (const auto& entry : commands) {
        out += "- id=" + std::to_string(entry.id) + " label=" + entry.label + " state=";
        switch (entry.state) {
        case RuntimeBridge::CommandState::Pending:
            out += "Pending";
            break;
        case RuntimeBridge::CommandState::Succeeded:
            out += "Succeeded";
            break;
        case RuntimeBridge::CommandState::Failed:
            out += "Failed";
            break;
        }
        if (!entry.message.empty()) {
            out += " msg=\"" + entry.message + "\"";
        }
        out += "\n";
    }
    return out;
}

} // namespace

TEST(SandboxGuiCommandQueueE2E, WorldDbLoadConsumeAndSaveSequenceSucceeds) {
    using namespace std::chrono_literals;
    using genesis::sandbox::gui::RuntimeBridge;

    RuntimeBridge bridge({});
    ASSERT_TRUE(bridge.start());
    bridge.setPaused(true);

    const auto loadFolder = repoPath("data/world_new");
    const auto saveFolder = uniqueTempFolder("genesis-world-db-save");

    nlohmann::json script = {
        {"name", "command-queue-e2e"},
        {"commands",
         nlohmann::json::array(
             {nlohmann::json{{"label", "load"}, {"action", "world.db.load"}, {"folder", loadFolder.string()}, {"waitForSuccess", true}},
              nlohmann::json{{"label", "consume"}, {"action", "resource.consume"}, {"interactionId", 1000}, {"amount", 10}, {"waitForSuccess", true}},
              nlohmann::json{{"label", "save"}, {"action", "world.db.save"}, {"folder", saveFolder.string()}, {"waitForSuccess", true}}})},
    };

    std::string error;
    ASSERT_TRUE(bridge.enqueueCommandSequence(script, "test", error)) << error;

    const auto deadline = std::chrono::steady_clock::now() + 10s;
    for (;;) {
        bridge.requestStep(1);
        std::this_thread::sleep_for(5ms);

        const auto commands = bridge.commandStatusSnapshot();
        const auto load = findCommand(commands, "load");
        const auto consume = findCommand(commands, "consume");
        const auto save = findCommand(commands, "save");

        if (load && consume && save && load->state != RuntimeBridge::CommandState::Pending &&
            consume->state != RuntimeBridge::CommandState::Pending && save->state != RuntimeBridge::CommandState::Pending) {
            EXPECT_EQ(load->state, RuntimeBridge::CommandState::Succeeded) << load->message;
            EXPECT_EQ(consume->state, RuntimeBridge::CommandState::Succeeded) << consume->message;
            EXPECT_EQ(save->state, RuntimeBridge::CommandState::Succeeded) << save->message;
            break;
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            FAIL() << "Timeout waiting for command sequence to finish\n" << dumpCommands(commands);
        }
    }

    EXPECT_TRUE(std::filesystem::exists(saveFolder / "world.json"));
    EXPECT_TRUE(std::filesystem::exists(saveFolder / "map_1.json"));
    EXPECT_TRUE(std::filesystem::exists(saveFolder / "map_2.json"));

    bridge.stop();
}

TEST(SandboxGuiCommandQueueE2E, WorldDbGenerateAutoLoadsAndAllowsConsume) {
    using namespace std::chrono_literals;
    using genesis::sandbox::gui::RuntimeBridge;

    RuntimeBridge bridge({});
    ASSERT_TRUE(bridge.start());
    bridge.setPaused(true);

    const auto configPath = repoPath("data/worldgen/default.toml");
    const auto generatedFolder = uniqueTempFolder("genesis-world-db-gen");
    const auto saveFolder = uniqueTempFolder("genesis-world-db-gen-save");

    nlohmann::json script = {
        {"name", "command-queue-worldgen-e2e"},
        {"commands",
         nlohmann::json::array(
             {nlohmann::json{{"label", "generate"}, {"action", "world.db.generate"}, {"configPath", configPath.string()}, {"seed", 123U}, {"folder", generatedFolder.string()}, {"waitForSuccess", true}},
              nlohmann::json{{"label", "consume"}, {"action", "resource.consume"}, {"interactionId", 1000}, {"amount", 1}, {"waitForSuccess", true}},
              nlohmann::json{{"label", "save"}, {"action", "world.db.save"}, {"folder", saveFolder.string()}, {"waitForSuccess", true}}})},
    };

    std::string error;
    ASSERT_TRUE(bridge.enqueueCommandSequence(script, "test", error)) << error;

    const auto deadline = std::chrono::steady_clock::now() + 20s;
    for (;;) {
        bridge.requestStep(1);
        std::this_thread::sleep_for(5ms);

        const auto commands = bridge.commandStatusSnapshot();
        const auto generate = findCommand(commands, "generate");
        const auto consume = findCommand(commands, "consume");
        const auto save = findCommand(commands, "save");

        if (generate && consume && save && generate->state != RuntimeBridge::CommandState::Pending &&
            consume->state != RuntimeBridge::CommandState::Pending && save->state != RuntimeBridge::CommandState::Pending) {
            EXPECT_EQ(generate->state, RuntimeBridge::CommandState::Succeeded) << generate->message;
            EXPECT_EQ(consume->state, RuntimeBridge::CommandState::Succeeded) << consume->message;
            EXPECT_EQ(save->state, RuntimeBridge::CommandState::Succeeded) << save->message;
            break;
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            FAIL() << "Timeout waiting for command sequence to finish\n" << dumpCommands(commands);
        }
    }

    EXPECT_TRUE(std::filesystem::exists(generatedFolder / "world.json"));
    EXPECT_TRUE(std::filesystem::exists(generatedFolder / "map_1.json"));
    EXPECT_TRUE(std::filesystem::exists(saveFolder / "world.json"));

    bridge.stop();
}
