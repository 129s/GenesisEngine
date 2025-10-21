#include "sandbox/gui/AppHost.hpp"

#include <algorithm>
#include <filesystem>
#include <cstdio>

#include "CommandUiHelpers.hpp"
#include "FilesystemHelpers.hpp"

namespace Genesis::Sandbox::Gui
{
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

    if (ui_state_.worldgen_command_id)
    {
        if (const auto* command = findCommand(*ui_state_.worldgen_command_id))
        {
            switch (command->state)
            {
            case RuntimeBridge::CommandState::Pending:
                ui_state_.world_command_status = commandStateSummary(*command);
                break;
            case RuntimeBridge::CommandState::Succeeded:
                ui_state_.world_command_status = commandStateSummary(*command);
                if (runtime_bridge_)
                {
                    if (auto latestOpt = runtime_bridge_->lastGeneration(); latestOpt && latestOpt->success)
                    {
                        const auto& latest = *latestOpt;
                        if (latest.outputPath)
                        {
                            const auto text = latest.outputPath->string();
                            std::snprintf(ui_state_.world_load_buffer.data(), ui_state_.world_load_buffer.size(), "%s", text.c_str());
                        }
                        if (ui_state_.worldgen_use_random_seed && latest.seed.value != 0)
                        {
                            ui_state_.worldgen_seed = latest.seed.value;
                        }
                    }
                }
                pushToast("World generated", ImVec4(0.62f, 0.84f, 0.58f, 1.0f));
                ui_state_.worldgen_command_id.reset();
                break;
            case RuntimeBridge::CommandState::Failed:
                ui_state_.world_command_status = commandStateSummary(*command);
                pushToast("World generation failed", ImVec4(0.95f, 0.45f, 0.45f, 1.0f));
                ui_state_.worldgen_command_id.reset();
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

    updateStatus(ui_state_.world_load_command_id, ui_state_.world_load_status, [this]() {
        resetSceneForNewWorld();
        pushToast("World loaded", ImVec4(0.62f, 0.84f, 0.58f, 1.0f));
    });

    updateStatus(ui_state_.world_save_command_id, ui_state_.world_save_status, [this]() {
        pushToast("World saved", ImVec4(0.62f, 0.84f, 0.58f, 1.0f));
    });
}

void AppHost::resetSceneForNewWorld()
{
    latest_snapshot_.reset();
    ui_state_.agent_trails.clear();
    ui_state_.inspector_selection_type = UiState::InspectorSelectionType::None;
    ui_state_.inspector_selected_primary = 0;
    ui_state_.inspector_selected_secondary = 0;
    ui_state_.inspector_highlight_node.reset();
    ui_state_.inspector_follow_selection = false;
    ui_state_.scene_selected_node = 0;
    ui_state_.scene_cam_offset_x = 0.0f;
    ui_state_.scene_cam_offset_y = 0.0f;
    ui_state_.scene_cam_zoom = 1.5f;
    resetMapViewCamera();
    ui_state_.map_selected_node.reset();
}

void AppHost::resetMapViewCamera()
{
    ui_state_.map_zoom = 1.0f;
    ui_state_.map_pan_x = 0.0f;
    ui_state_.map_pan_y = 0.0f;
}

void AppHost::refreshDefaultWorldgenConfig()
{
    std::fill(ui_state_.worldgen_config_buffer.begin(), ui_state_.worldgen_config_buffer.end(), '\0');
    std::fill(ui_state_.worldgen_output_buffer.begin(), ui_state_.worldgen_output_buffer.end(), '\0');
    std::fill(ui_state_.world_load_buffer.begin(), ui_state_.world_load_buffer.end(), '\0');
    std::fill(ui_state_.world_save_buffer.begin(), ui_state_.world_save_buffer.end(), '\0');
    std::fill(ui_state_.command_script_buffer.begin(), ui_state_.command_script_buffer.end(), '\0');
    ui_state_.world_load_status.clear();
    ui_state_.world_save_status.clear();
    ui_state_.world_command_status.clear();
    ui_state_.command_script_status.clear();
    ui_state_.worldgen_command_id.reset();
    ui_state_.world_load_command_id.reset();
    ui_state_.world_save_command_id.reset();

    const std::filesystem::path defaultConfig{"data/worldgen/default.toml"};
    if (auto resolved = locateAsset(defaultConfig); !resolved.empty())
    {
        resolved.make_preferred();
        const auto text = resolved.string();
        std::snprintf(ui_state_.worldgen_config_buffer.data(), ui_state_.worldgen_config_buffer.size(), "%s", text.c_str());
    }
    else if (std::filesystem::exists(defaultConfig))
    {
        auto preferred = defaultConfig;
        preferred.make_preferred();
        const auto text = preferred.string();
        std::snprintf(ui_state_.worldgen_config_buffer.data(), ui_state_.worldgen_config_buffer.size(), "%s", text.c_str());
    }

    const std::filesystem::path defaultOutput{"data/world/generated/generated_world.json"};
    auto preferredOutput = defaultOutput;
    preferredOutput.make_preferred();
    const auto outputText = preferredOutput.string();
    std::snprintf(ui_state_.worldgen_output_buffer.data(), ui_state_.worldgen_output_buffer.size(), "%s", outputText.c_str());
    std::snprintf(ui_state_.world_load_buffer.data(), ui_state_.world_load_buffer.size(), "%s", outputText.c_str());
    std::snprintf(ui_state_.world_save_buffer.data(), ui_state_.world_save_buffer.size(), "%s", outputText.c_str());
}

} // namespace Genesis::Sandbox::Gui
