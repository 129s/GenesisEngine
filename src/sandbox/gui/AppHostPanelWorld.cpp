#include "sandbox/gui/AppHost.hpp"

#include <algorithm>
#include <filesystem>
#include <cstdio>

#include "FilesystemHelpers.hpp"

namespace Genesis::Sandbox::Gui
{
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
    ui_state_.map_auto_centered = false;
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
