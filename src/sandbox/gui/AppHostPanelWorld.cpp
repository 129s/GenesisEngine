#include "sandbox/gui/AppHost.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "CommandUiHelpers.hpp"
#include "FilesystemHelpers.hpp"

namespace Genesis::Sandbox::Gui
{
using json = nlohmann::json;

void AppHost::drawWelcomePanel()
{
    ImGui::Begin("Welcome", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::TextUnformatted("Genesis Sandbox GUI · RuntimeBridge");
    ImGui::Separator();

    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Average %.2f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::ColorEdit4("Clear Color", clear_color_.data(), ImGuiColorEditFlags_NoInputs);

    ImGui::Separator();
    if (runtime_bridge_)
    {
        const bool paused = runtime_bridge_->paused();
        ImGui::Text("Playback: %s", paused ? "Paused" : "Running");
        if (latest_snapshot_)
        {
            const auto& tick = latest_snapshot_->telemetry;
            ImGui::Text("Step: %llu", static_cast<unsigned long long>(tick.step));
            ImGui::Text("Agents: %zu", tick.agents.size());
            ImGui::Text("Resources: %zu", tick.resources.size());
        }
        else
        {
            ImGui::TextUnformatted("Waiting for first snapshot…");
        }
    }
    else
    {
        ImGui::TextUnformatted("RuntimeBridge unavailable.");
    }

    ImGui::Separator();
    ImGui::TextWrapped(
        "Focus: RuntimeBridge advances the simulation in a background thread, exposes pause/step/speed controls, and "
        "feeds the world/telemetry panels with the latest snapshot. Use the toolbar above (or F5/F6/F7 hotkeys) for "
        "playback control; VSync, logging and telemetry toggles are also available via toolbar shortcuts.");

    ImGui::End();
}

void AppHost::drawWorldGenerationPanel()
{
    if (!show_worldgen_panel_)
    {
        return;
    }

    if (!ImGui::Begin("World Generation", &show_worldgen_panel_))
    {
        ImGui::End();
        return;
    }

    const bool bridgeReady = runtime_bridge_ != nullptr;
    std::vector<RuntimeBridge::CommandProgress> commandStatuses;
    if (bridgeReady)
    {
        commandStatuses = runtime_bridge_->commandStatusSnapshot();
        refreshCommandStatusTexts(commandStatuses);
        std::unordered_set<std::uint64_t> currentIds;
        currentIds.reserve(commandStatuses.size());
        for (const auto& cmd : commandStatuses)
        {
            currentIds.insert(cmd.id);
        }
        for (auto it = world_queue_hidden_completed_.begin(); it != world_queue_hidden_completed_.end();)
        {
            if (!currentIds.contains(*it))
            {
                it = world_queue_hidden_completed_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    ImGui::TextUnformatted("World Generation → command-queue driven generate/load/save.");
    ImGui::Separator();

    ImGui::InputText("Config Path", worldgen_config_buffer_.data(), worldgen_config_buffer_.size());
    ImGui::InputText("Output Path", worldgen_output_buffer_.data(), worldgen_output_buffer_.size());

    if (ImGui::Checkbox("Random Seed", &worldgen_use_random_seed_))
    {
        if (worldgen_use_random_seed_)
        {
            worldgen_seed_ = static_cast<std::uint64_t>(std::random_device{}());
        }
    }

    if (worldgen_use_random_seed_)
    {
        ImGui::SameLine();
        if (ImGui::Button("Refresh Seed"))
        {
            worldgen_seed_ = static_cast<std::uint64_t>(std::random_device{}());
        }
        ImGui::SameLine();
        ImGui::Text("Current: %llu", static_cast<unsigned long long>(worldgen_seed_));
    }
    else
    {
        ImGui::InputScalar("Seed", ImGuiDataType_U64, &worldgen_seed_);
    }

    const std::string configInput(worldgen_config_buffer_.data());
    const std::string outputInput(worldgen_output_buffer_.data());
    const bool hasConfig = !configInput.empty();
    if (!hasConfig)
    {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1.0f), "Please provide config path");
    }

    if (!bridgeReady || !hasConfig)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Generate World"))
    {
        if (bridgeReady)
        {
            json command = {
                {"action", "world.generate"},
                {"configPath", configInput},
            };
            if (!worldgen_use_random_seed_)
            {
                command["seed"] = worldgen_seed_;
            }
            if (!outputInput.empty())
            {
                command["outputPath"] = outputInput;
            }

            std::string error;
            if (auto id = runtime_bridge_->enqueueCommandFromJson(command, "ui", error))
            {
                worldgen_command_id_ = id;
                world_command_status_ = "Command enqueued (#" + std::to_string(*id) + ")";
            }
            else
            {
                world_command_status_ = "Submit failed: " + error;
            }
        }
    }
    if (!bridgeReady || !hasConfig)
    {
        ImGui::EndDisabled();
    }
    if (!world_command_status_.empty())
    {
        ImGui::TextWrapped("%s", world_command_status_.c_str());
    }

    if (bridgeReady)
    {
        if (auto resultOpt = runtime_bridge_->lastGeneration(); resultOpt)
        {
            const auto& result = *resultOpt;
            ImGui::Separator();
            if (result.success)
            {
                ImGui::Text("Last generation succeeded");
                ImGui::BulletText("Config: %s", result.configPath.string().c_str());
                ImGui::BulletText("Seed: %llu", static_cast<unsigned long long>(result.seed.value));
                ImGui::BulletText("Locations: %zu · Edges: %zu", result.locationCount, result.edgeCount);
                ImGui::BulletText("Duration: %.2f ms", result.durationMs);
                if (result.outputPath)
                {
                    ImGui::BulletText("Output file: %s", result.outputPath->string().c_str());
                }
                else
                {
                    ImGui::BulletText("Output file: not specified (kept in memory)");
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "Generation failed: %s", result.error.c_str());
            }

            if (!result.logs.empty())
            {
                if (ImGui::BeginChild("WorldGenLogs", ImVec2(0.0f, 180.0f), true))
                {
                    for (const auto& entry : result.logs)
                    {
                        ImGui::TextUnformatted(entry.message.c_str());
                    }
                }
                ImGui::EndChild();
            }
        }
    }

    ImGui::Separator();
    ImGui::InputText("Load Path", world_load_buffer_.data(), world_load_buffer_.size());
    const std::string loadInput(world_load_buffer_.data());
    if (loadInput.empty())
    {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1.0f), "Please provide load path");
    }

    if (!bridgeReady || loadInput.empty())
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Load World"))
    {
        if (bridgeReady)
        {
            json command = {
                {"action", "world.load"},
                {"path", loadInput},
            };
            std::string error;
            if (auto id = runtime_bridge_->enqueueCommandFromJson(command, "ui", error))
            {
                world_load_command_id_ = id;
                world_load_status_ = "Load enqueued (#" + std::to_string(*id) + ")";
            }
            else
            {
                world_load_status_ = "Load failed: " + error;
            }
        }
    }
    if (!bridgeReady || loadInput.empty())
    {
        ImGui::EndDisabled();
    }
    if (!world_load_status_.empty())
    {
        ImGui::TextWrapped("%s", world_load_status_.c_str());
    }

    ImGui::InputText("Save Path", world_save_buffer_.data(), world_save_buffer_.size());
    const std::string saveInput(world_save_buffer_.data());
    if (saveInput.empty())
    {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1.0f), "Please provide save path");
    }

    if (!bridgeReady || saveInput.empty())
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Save World"))
    {
        if (bridgeReady)
        {
            json command = {
                {"action", "world.save"},
                {"path", saveInput},
            };
            std::string error;
            if (auto id = runtime_bridge_->enqueueCommandFromJson(command, "ui", error))
            {
                world_save_command_id_ = id;
                world_save_status_ = "Save enqueued (#" + std::to_string(*id) + ")";
            }
            else
            {
                world_save_status_ = "Save failed: " + error;
            }
        }
    }
    if (!bridgeReady || saveInput.empty())
    {
        ImGui::EndDisabled();
    }
    if (!world_save_status_.empty())
    {
        ImGui::TextWrapped("%s", world_save_status_.c_str());
    }

    ImGui::Separator();
    ImGui::InputText("Command Script", command_script_buffer_.data(), command_script_buffer_.size());
    const std::string scriptPath(command_script_buffer_.data());
    if (scriptPath.empty())
    {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1.0f), "Please provide script path");
    }

    if (!bridgeReady)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Run Script"))
    {
        if (scriptPath.empty())
        {
            command_script_status_ = "Please provide script path";
        }
        else if (bridgeReady)
        {
            std::string error;
            if (runtime_bridge_->enqueueCommandScript(std::filesystem::path(scriptPath), "script", error))
            {
                command_script_status_ = "Script enqueued";
            }
            else
            {
                command_script_status_ = "Script execution failed: " + error;
            }
        }
    }
    if (!bridgeReady)
    {
        ImGui::EndDisabled();
    }
    if (!command_script_status_.empty())
    {
        ImGui::TextWrapped("%s", command_script_status_.c_str());
    }

    ImGui::Separator();
    ImGui::SetNextItemOpen(false, ImGuiCond_Once);
    if (ImGui::CollapsingHeader("Command Queue"))
    {
        ImGui::Checkbox("Show Pending", &world_queue_show_pending_);
        ImGui::SameLine();
        ImGui::Checkbox("Show Succeeded", &world_queue_show_succeeded_);
        ImGui::SameLine();
        ImGui::Checkbox("Show Failed", &world_queue_show_failed_);

        bool hasVisibleCompleted = false;
        for (const auto& cmd : commandStatuses)
        {
            if (cmd.state == RuntimeBridge::CommandState::Succeeded && !world_queue_hidden_completed_.contains(cmd.id))
            {
                hasVisibleCompleted = true;
                break;
            }
        }

        ImGui::SameLine(0.0f, 18.0f);
        if (!hasVisibleCompleted)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Clear Completed"))
        {
            for (const auto& cmd : commandStatuses)
            {
                if (cmd.state == RuntimeBridge::CommandState::Succeeded)
                {
                    world_queue_hidden_completed_.insert(cmd.id);
                }
            }
        }
        if (!hasVisibleCompleted)
        {
            ImGui::EndDisabled();
        }

        std::vector<RuntimeBridge::CommandProgress> filtered;
        filtered.reserve(commandStatuses.size());
        for (const auto& cmd : commandStatuses)
        {
            if (cmd.state == RuntimeBridge::CommandState::Pending && !world_queue_show_pending_)
            {
                continue;
            }
            if (cmd.state == RuntimeBridge::CommandState::Succeeded)
            {
                if (!world_queue_show_succeeded_)
                {
                    continue;
                }
                if (world_queue_hidden_completed_.contains(cmd.id))
                {
                    continue;
                }
            }
            if (cmd.state == RuntimeBridge::CommandState::Failed && !world_queue_show_failed_)
            {
                continue;
            }
            filtered.push_back(cmd);
        }

        if (filtered.empty())
        {
            ImGui::TextUnformatted("No command entries");
        }
        else if (ImGui::BeginChild("CommandQueueView", ImVec2(0.0f, 220.0f), true))
        {
            if (ImGui::BeginTable("CommandQueueTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
            {
                ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Label");
                ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Notes");
                ImGui::TableHeadersRow();

                for (const auto& cmd : filtered)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("#%llu", static_cast<unsigned long long>(cmd.id));

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(cmd.label.c_str());

                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(cmd.source.c_str());

                    ImGui::TableSetColumnIndex(3);
                    const ImVec4 color = commandStateColor(cmd.state);
                    ImGui::TextColored(color, "%s", commandStateLabel(cmd.state));

                    ImGui::TableSetColumnIndex(4);
                    if (!cmd.message.empty())
                    {
                        ImGui::TextWrapped("%s", cmd.message.c_str());
                    }
                    else if (cmd.payloadJson.has_value())
                    {
                        ImGui::TextDisabled("%s", cmd.payloadJson->c_str());
                    }
                    else
                    {
                        ImGui::TextDisabled("-");
                    }
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
        }
    }

    ImGui::End();
}

void AppHost::refreshCommandStatusTexts(const std::vector<RuntimeBridge::CommandProgress>& commands)
{
    auto findCommand = [&commands](std::uint64_t id) -> const RuntimeBridge::CommandProgress* {
        for (const auto& command : commands)
        {
            if (command.id == id)
            {
                return &command;
            }
        }
        return nullptr;
    };

    if (worldgen_command_id_)
    {
        if (const auto* command = findCommand(*worldgen_command_id_))
        {
            switch (command->state)
            {
            case RuntimeBridge::CommandState::Pending:
                world_command_status_ = commandStateSummary(*command);
                break;
            case RuntimeBridge::CommandState::Succeeded:
                world_command_status_ = commandStateSummary(*command);
                if (runtime_bridge_)
                {
                    if (auto latestOpt = runtime_bridge_->lastGeneration(); latestOpt && latestOpt->success)
                    {
                        const auto& latest = *latestOpt;
                        if (latest.outputPath)
                        {
                            const auto text = latest.outputPath->string();
                            std::snprintf(world_load_buffer_.data(), world_load_buffer_.size(), "%s", text.c_str());
                        }
                        if (worldgen_use_random_seed_ && latest.seed.value != 0)
                        {
                            worldgen_seed_ = latest.seed.value;
                        }
                    }
                }
                pushToast("World generated", ImVec4(0.62f, 0.84f, 0.58f, 1.0f));
                worldgen_command_id_.reset();
                break;
            case RuntimeBridge::CommandState::Failed:
                world_command_status_ = commandStateSummary(*command);
                pushToast("World generation failed", ImVec4(0.95f, 0.45f, 0.45f, 1.0f));
                worldgen_command_id_.reset();
                break;
            }
        }
    }

    auto updateStatus = [&](std::optional<std::uint64_t>& idHolder, std::string& statusText, auto onSuccess) {
        if (!idHolder)
        {
            return;
        }
        if (const auto* command = findCommand(*idHolder))
        {
            switch (command->state)
            {
            case RuntimeBridge::CommandState::Pending:
                statusText = commandStateSummary(*command);
                break;
            case RuntimeBridge::CommandState::Succeeded:
                statusText = commandStateSummary(*command);
                onSuccess();
                idHolder.reset();
                break;
            case RuntimeBridge::CommandState::Failed:
                statusText = commandStateSummary(*command);
                idHolder.reset();
                break;
            }
        }
    };

    updateStatus(world_load_command_id_, world_load_status_, [this]() {
        resetSceneForNewWorld();
        pushToast("World loaded", ImVec4(0.62f, 0.84f, 0.58f, 1.0f));
    });

    updateStatus(world_save_command_id_, world_save_status_, [this]() {
        pushToast("World saved", ImVec4(0.62f, 0.84f, 0.58f, 1.0f));
    });
}

void AppHost::resetSceneForNewWorld()
{
    latest_snapshot_.reset();
    agent_trails_.clear();
    inspector_selection_type_ = InspectorSelectionType::None;
    inspector_selected_primary_ = 0;
    inspector_selected_secondary_ = 0;
    inspector_highlight_node_.reset();
    inspector_follow_selection_ = false;
    scene_selected_node_ = 0;
    scene_cam_offset_x_ = 0.0f;
    scene_cam_offset_y_ = 0.0f;
    scene_cam_zoom_ = 1.5f;
    resetMapViewCamera();
    map_selected_node_.reset();
}

void AppHost::resetMapViewCamera()
{
    map_zoom_ = 1.0f;
    map_pan_x_ = 0.0f;
    map_pan_y_ = 0.0f;
}

void AppHost::refreshDefaultWorldgenConfig()
{
    std::fill(worldgen_config_buffer_.begin(), worldgen_config_buffer_.end(), '\0');
    std::fill(worldgen_output_buffer_.begin(), worldgen_output_buffer_.end(), '\0');
    std::fill(world_load_buffer_.begin(), world_load_buffer_.end(), '\0');
    std::fill(world_save_buffer_.begin(), world_save_buffer_.end(), '\0');
    std::fill(command_script_buffer_.begin(), command_script_buffer_.end(), '\0');
    world_load_status_.clear();
    world_save_status_.clear();
    world_command_status_.clear();
    command_script_status_.clear();
    worldgen_command_id_.reset();
    world_load_command_id_.reset();
    world_save_command_id_.reset();

    const std::filesystem::path defaultConfig{"data/worldgen/default.toml"};
    if (auto resolved = locateAsset(defaultConfig); !resolved.empty())
    {
        resolved.make_preferred();
        const auto text = resolved.string();
        std::snprintf(worldgen_config_buffer_.data(), worldgen_config_buffer_.size(), "%s", text.c_str());
    }
    else if (std::filesystem::exists(defaultConfig))
    {
        auto preferred = defaultConfig;
        preferred.make_preferred();
        const auto text = preferred.string();
        std::snprintf(worldgen_config_buffer_.data(), worldgen_config_buffer_.size(), "%s", text.c_str());
    }

    const std::filesystem::path defaultOutput{"data/world/generated/generated_world.json"};
    auto preferredOutput = defaultOutput;
    preferredOutput.make_preferred();
    const auto outputText = preferredOutput.string();
    std::snprintf(worldgen_output_buffer_.data(), worldgen_output_buffer_.size(), "%s", outputText.c_str());
    std::snprintf(world_load_buffer_.data(), world_load_buffer_.size(), "%s", outputText.c_str());
    std::snprintf(world_save_buffer_.data(), world_save_buffer_.size(), "%s", outputText.c_str());
}

} // namespace Genesis::Sandbox::Gui
