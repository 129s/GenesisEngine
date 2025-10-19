#pragma once

#include <array>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "sandbox/gui/RuntimeBridge.hpp"

#include <imgui.h>

struct GLFWwindow;

namespace Genesis::Sandbox::Gui
{

class ImGuiLogSink;

struct AppHostConfig
{
    int width = 1600;
    int height = 900;
    std::string title = "Genesis Sandbox GUI";
    bool vsync = true;
};

class AppHost
{
public:
    explicit AppHost(AppHostConfig config = {});
    ~AppHost();

    AppHost(const AppHost&) = delete;
    AppHost& operator=(const AppHost&) = delete;
    AppHost(AppHost&&) = delete;
    AppHost& operator=(AppHost&&) = delete;

    bool initialize();
    void run();

private:
    bool initializeGlfw();
    bool initializeImGui();
    void shutdown();

    void beginFrame();
    void renderGui();
    void endFrame();

    void drawDockspace();
    void drawMainMenuBar();
    void drawControlToolbar();
    void drawWelcomePanel();
    void drawWorldGenerationPanel();
    void drawWorldViewPanel();
    void drawSceneViewPanel();
    void drawTelemetryPanel();
    void drawInspectorPanel();
    void drawLogPanel();
    void drawStatusBar();
    void drawToasts();
    void handleShortcuts();

    void updateRuntimeSnapshot();
    void updateAgentTrails(const RuntimeBridge::Snapshot& snapshot);
    void resetSceneForNewWorld();
    void resetMapViewCamera();
    void refreshCommandStatusTexts(const std::vector<RuntimeBridge::CommandProgress>& commands);
    void refreshDefaultWorldgenConfig();

    AppHostConfig config_;
    GLFWwindow* window_{nullptr};
    bool initialized_{false};
    std::array<float, 4> clear_color_;
    bool glfw_initialized_{false};
    bool imgui_initialized_{false};
    std::unique_ptr<RuntimeBridge> runtime_bridge_;
    std::optional<RuntimeBridge::Snapshot> latest_snapshot_;
    double speed_multiplier_ui_{1.0};
    bool show_inspector_{true};
    bool show_world_view_{true};
    bool show_scene_view_{true};
    bool show_telemetry_{true};
    bool show_logs_{true};
    bool show_worldgen_panel_{true};
    bool log_auto_scroll_{true};
    std::shared_ptr<ImGuiLogSink> log_sink_;
    std::size_t log_last_line_count_{0};
    bool show_agent_overlay_{true};
    bool show_agent_trails_{false};
    bool map_interpolate_{true};
    std::size_t agent_trail_samples_{24};
    std::unordered_map<std::uint32_t, std::deque<RuntimeBridge::Vector2>> agent_trails_;
    std::optional<std::uint32_t> map_selected_node_;
    float map_zoom_{1.0f};
    float map_pan_x_{0.0f};
    float map_pan_y_{0.0f};

    // Toast notifications (top-right)
    struct Toast
    {
        std::string text;
        ImVec4 color{0.9f, 0.9f, 0.9f, 1.0f};
        double expiresAt{0.0}; // ImGui::GetTime()
    };
    std::deque<Toast> toasts_;
    void pushToast(const std::string& text, const ImVec4& color, double lifetimeSec = 3.0);

    // Scene View state
    std::uint32_t scene_selected_node_{0};
    float scene_cam_offset_x_{0.0f};
    float scene_cam_offset_y_{0.0f};
    float scene_cam_zoom_{1.5f};
    bool scene_show_grid_{true};
    bool scene_show_anchors_{true};
    bool scene_show_resources_{true};

    enum class InspectorSelectionType
    {
        None,
        Agent,
        Resource,
        Node
    };

    std::array<char, 128> inspector_search_buffer_{};
    InspectorSelectionType inspector_selection_type_{InspectorSelectionType::None};
    std::uint32_t inspector_selected_primary_{0};
    std::uint32_t inspector_selected_secondary_{0};
    std::optional<std::uint32_t> inspector_highlight_node_;
    bool inspector_follow_selection_{false};

    // World generation UI state
    std::array<char, 512> worldgen_config_buffer_{};
    std::array<char, 512> worldgen_output_buffer_{};
    std::array<char, 512> world_load_buffer_{};
    std::array<char, 512> world_save_buffer_{};
    std::array<char, 512> command_script_buffer_{};
    bool worldgen_use_random_seed_{true};
    std::uint64_t worldgen_seed_{0};
    std::optional<std::uint64_t> worldgen_command_id_;
    std::optional<std::uint64_t> world_load_command_id_;
    std::optional<std::uint64_t> world_save_command_id_;
    std::string world_command_status_;
    std::string world_load_status_;
    std::string world_save_status_;
    std::string command_script_status_;
    bool world_queue_show_pending_{true};
    bool world_queue_show_succeeded_{true};
    bool world_queue_show_failed_{true};
    std::unordered_set<std::uint64_t> world_queue_hidden_completed_;
};

} // namespace Genesis::Sandbox::Gui
