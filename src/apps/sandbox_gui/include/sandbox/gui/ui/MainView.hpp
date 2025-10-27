#pragma once

#include "sandbox/gui/presenter/MainViewPresenters.hpp"
#include "sandbox/gui/style/LayoutMetrics.hpp"
#include "sandbox/gui/ui/UiContext.hpp"

#include <optional>

namespace Genesis::Sandbox::Gui
{

struct SceneViewportRenderState
{
    const SceneNodeDetails* details{nullptr};
    SceneNodeGridInfo grid{};
    float viewOriginX{0.0f};
    float viewOriginY{0.0f};
    float viewWidthTiles{0.0f};
    float viewHeightTiles{0.0f};
    float cellPx{1.0f};
    ImVec2 canvasPos{0.0f, 0.0f};
    ImVec2 canvasExtent{0.0f, 0.0f};
    float canvasMargin{24.0f};
};

class InspectorView;

class MainView
{
public:
    void render(UiContext& ctx, InspectorView& inspector);

private:
    ScenePresenter scene_presenter_;
    WorldPresenter world_presenter_;
    MonitorPresenter monitor_presenter_;

    void drawSceneTab(UiContext& ctx, InspectorView& inspector);
    void drawSceneUnified(UiContext& ctx, InspectorView& inspector);
    bool drawSceneToolbar(UiContext& ctx, const SceneNodeDetails* details);
    std::optional<SceneViewportRenderState> drawSceneViewport(UiContext& ctx,
                                                              const ScenePresenterInput& presenterInput,
                                                              const SceneNodeViewModel& nodeVm,
                                                              bool resetRequested);
    void drawWorldTab(UiContext& ctx);
    void drawMonitorTab(UiContext& ctx);
    void drawSettingsTab(UiContext& ctx);
    void drawMonitorTelemetry(UiContext& ctx, const Style::Layout::CardLayoutConfig& layout);
    void drawMonitorLog(UiContext& ctx, const Style::Layout::CardLayoutConfig& layout);
};

class InspectorView
{
public:
    void render(UiContext& ctx, const std::optional<SceneViewportRenderState>& viewportState);
};

} // namespace Genesis::Sandbox::Gui
