#pragma once

#include <optional>
#include <vector>

#include "sandbox/gui/ui/UiState.hpp"

namespace Genesis::Sandbox::Gui
{

class AppHost;
struct AppHostConfig;
class WorldCommandController;

struct UiContext
{
    AppHost& host;
    UiState& state;
    RuntimeBridge* runtime_bridge;
    std::optional<RuntimeBridge::Snapshot>& latest_snapshot;
    double& speed_multiplier_ui;
    AppHostConfig& config;
    WorldCommandController& world_commands;

    void pushToast(const std::string& text, const ImVec4& color, double lifetime_sec = 3.0);
    void resetSceneForNewWorld();
    void resetMapViewCamera();
    void updateWorldCommandStatuses(const std::vector<RuntimeBridge::CommandProgress>& commands);
};

} // namespace Genesis::Sandbox::Gui
