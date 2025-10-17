#pragma once

#include <array>
#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "sandbox/gui/RuntimeBridge.hpp"

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
    void drawWelcomePanel();
    void drawWorldViewPanel();
    void drawSceneViewPanel();
    void drawTelemetryPanel();
    void drawLogPanel();
    void drawStatusBar();

    void updateRuntimeSnapshot();
    void updateAgentTrails(const RuntimeBridge::Snapshot& snapshot);

    AppHostConfig config_;
    GLFWwindow* window_{nullptr};
    bool initialized_{false};
    std::array<float, 4> clear_color_;
    bool glfw_initialized_{false};
    bool imgui_initialized_{false};
    std::unique_ptr<RuntimeBridge> runtime_bridge_;
    std::optional<RuntimeBridge::Snapshot> latest_snapshot_;
    double speed_multiplier_ui_{1.0};
    bool show_world_view_{true};
    bool show_scene_view_{false};
    bool show_telemetry_{true};
    bool show_logs_{true};
    bool log_auto_scroll_{true};
    std::shared_ptr<ImGuiLogSink> log_sink_;
    std::size_t log_last_line_count_{0};
    bool show_agent_overlay_{true};
    bool show_agent_trails_{false};
    std::size_t agent_trail_samples_{24};
    std::unordered_map<std::uint32_t, std::deque<RuntimeBridge::Vector2>> agent_trails_;

    // Scene View state
    std::uint32_t scene_selected_node_{0};
    float scene_cam_offset_x_{0.0f};
    float scene_cam_offset_y_{0.0f};
    float scene_cam_zoom_{1.0f};
};

} // namespace Genesis::Sandbox::Gui
