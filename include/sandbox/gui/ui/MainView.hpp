#pragma once

#include "sandbox/gui/ui/UiContext.hpp"

namespace Genesis::Sandbox::Gui
{

class MainView
{
public:
    void render(UiContext& ctx);

private:
    void drawSceneTab(UiContext& ctx);
    void drawWorldTab(UiContext& ctx);
    void drawMonitorTab(UiContext& ctx);
    void drawSettingsTab(UiContext& ctx);
    void drawSceneWorldMap(UiContext& ctx);
    void drawSceneNode(UiContext& ctx);
    void drawMonitorTelemetry(UiContext& ctx);
    void drawMonitorLog(UiContext& ctx);
};

class InspectorView
{
public:
    void render(UiContext& ctx);
};

} // namespace Genesis::Sandbox::Gui
