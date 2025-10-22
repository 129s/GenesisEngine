#pragma once

#include "sandbox/gui/presenter/MainViewPresenters.hpp"
#include "sandbox/gui/style/LayoutMetrics.hpp"
#include "sandbox/gui/ui/UiContext.hpp"

namespace Genesis::Sandbox::Gui
{

class MainView
{
public:
    void render(UiContext& ctx);

private:
    ScenePresenter scene_presenter_;
    WorldPresenter world_presenter_;
    MonitorPresenter monitor_presenter_;

    void drawSceneTab(UiContext& ctx);
    void drawSceneUnified(UiContext& ctx);
    void drawWorldTab(UiContext& ctx);
    void drawMonitorTab(UiContext& ctx);
    void drawSettingsTab(UiContext& ctx);
    void drawMonitorTelemetry(UiContext& ctx, const Style::Layout::CardLayoutConfig& layout);
    void drawMonitorLog(UiContext& ctx, const Style::Layout::CardLayoutConfig& layout);
};

class InspectorView
{
public:
    void render(UiContext& ctx);
};

} // namespace Genesis::Sandbox::Gui
