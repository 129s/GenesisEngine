#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <imgui.h>

#include "sandbox/gui/RuntimeBridge.hpp"

namespace Genesis::Sandbox::Gui
{

class ImGuiLogSink;

enum class BrowserSection
{
    Scene,
    World,
    Monitor,
    LayoutsThemes
};

enum class MainViewTab
{
    Scene,
    World,
    Monitor,
    Settings
};

enum class SceneSelectionTool
{
    Any,
    Node,
    Tile
};

struct SceneTileSelection
{
    std::uint32_t nodeId{0};
    int tileX{0};
    int tileY{0};
};

struct UiState
{
    bool show_inspector{true};
    BrowserSection browser_active_section{BrowserSection::Scene};
    MainViewTab main_view_active_tab{MainViewTab::Scene};
    SceneSelectionTool scene_selection_tool{SceneSelectionTool::Any};

    bool log_auto_scroll{true};
    std::shared_ptr<ImGuiLogSink> log_sink;
    std::size_t log_last_line_count{0};

    bool show_agent_overlay{true};
    bool show_agent_trails{false};
    bool map_interpolate{true};
    std::size_t agent_trail_samples{24};
    std::unordered_map<std::uint32_t, std::deque<RuntimeBridge::Vector2>> agent_trails;
    std::optional<std::uint32_t> map_selected_node;
    float map_zoom{1.0f};
    float map_pan_x{0.0f};
    float map_pan_y{0.0f};
    bool scene_show_graph{true};
    bool scene_show_ruler{false};
    std::optional<RuntimeBridge::Vector2> scene_ruler_anchor;
    std::optional<std::uint32_t> scene_focus_node_request;
    std::array<char, 128> browser_search_buffer{};
    std::unordered_set<std::uint32_t> browser_scene_expanded_nodes;

    struct Toast
    {
        std::string text;
        ImVec4 color{0.9f, 0.9f, 0.9f, 1.0f};
        double expires_at{0.0}; // ImGui::GetTime()
    };

    std::deque<Toast> toasts;
    void pushToast(const std::string& text, const ImVec4& color, double lifetime_sec = 3.0);
    void pruneExpiredToasts(double now);

    // Scene View state
    std::uint32_t scene_selected_node{0};
    std::optional<SceneTileSelection> scene_tile_selection;
    std::uint32_t scene_camera_node{0};
    float scene_cam_offset_x{0.0f};
    float scene_cam_offset_y{0.0f};
    float scene_cam_zoom{1.0f};
    bool scene_show_grid{true};
    bool scene_show_anchors{true};
    bool scene_show_resources{true};

    enum class InspectorSelectionType
    {
        None,
        Agent,
        Resource,
        Node
    };

    std::array<char, 128> inspector_search_buffer{};
    InspectorSelectionType inspector_selection_type{InspectorSelectionType::None};
    std::uint32_t inspector_selected_primary{0};
    std::uint32_t inspector_selected_secondary{0};
    std::optional<std::uint32_t> inspector_highlight_node;
    bool inspector_follow_selection{false};

    // World generation UI state
    std::array<char, 512> worldgen_config_buffer{};
    std::array<char, 512> worldgen_output_buffer{};
    std::array<char, 512> world_load_buffer{};
    std::array<char, 512> world_save_buffer{};
    std::array<char, 512> command_script_buffer{};
    bool worldgen_use_random_seed{true};
    std::uint64_t worldgen_seed{0};
    std::optional<std::uint64_t> worldgen_command_id;
    std::optional<std::uint64_t> world_load_command_id;
    std::optional<std::uint64_t> world_save_command_id;
    std::string world_command_status;
    std::string world_load_status;
    std::string world_save_status;
    std::string command_script_status;
};

} // namespace Genesis::Sandbox::Gui
