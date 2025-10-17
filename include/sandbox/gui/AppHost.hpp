#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>

#include "sandbox/gui/RuntimeBridge.hpp"

struct GLFWwindow;

namespace Genesis::Sandbox::Gui
{

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
    void drawTelemetryPanel();
    void drawStatusBar();

    void updateRuntimeSnapshot();

    AppHostConfig config_;
    GLFWwindow* window_{nullptr};
    bool initialized_{false};
    bool show_demo_window_{true};
    std::array<float, 4> clear_color_;
    bool glfw_initialized_{false};
    bool imgui_initialized_{false};
    std::unique_ptr<RuntimeBridge> runtime_bridge_;
    std::optional<RuntimeBridge::Snapshot> latest_snapshot_;
    double speed_multiplier_ui_{1.0};
    bool show_world_view_{true};
    bool show_telemetry_{true};
};

} // namespace Genesis::Sandbox::Gui
