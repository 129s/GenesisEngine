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
#include "sandbox/gui/controller/WorldCommandController.hpp"
#include "sandbox/gui/ui/UiContext.hpp"
#include "sandbox/gui/ui/StatusBarView.hpp"
#include "sandbox/gui/ui/ControlBarView.hpp"
#include "sandbox/gui/ui/BrowserView.hpp"
#include "sandbox/gui/ui/MainView.hpp"

#include <imgui.h>

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
    void drawToasts();
    void handleShortcuts();

    void updateRuntimeSnapshot();
    void updateAgentTrails(const RuntimeBridge::Snapshot& snapshot);
    void resetSceneForNewWorld();
    void resetMapViewCamera();
    void refreshDefaultWorldgenConfig();

    friend struct UiContext;

    AppHostConfig config_;
    GLFWwindow* window_{nullptr};
    bool initialized_{false};
    std::array<float, 4> clear_color_;
    bool glfw_initialized_{false};
    bool imgui_initialized_{false};
    bool dock_layout_initialized_{false};
    std::unique_ptr<RuntimeBridge> runtime_bridge_;
    std::optional<RuntimeBridge::Snapshot> latest_snapshot_;
    double speed_multiplier_ui_{1.0};

    UiState ui_state_;
    WorldCommandController world_command_controller_;
    UiContext ui_context_;
    StatusBarView status_bar_view_;
    ControlBarView control_bar_view_;
    BrowserView browser_view_;
    MainView main_view_;
    InspectorView inspector_view_;
    void pushToast(const std::string& text, const ImVec4& color, double lifetimeSec = 3.0);

};

} // namespace Genesis::Sandbox::Gui
