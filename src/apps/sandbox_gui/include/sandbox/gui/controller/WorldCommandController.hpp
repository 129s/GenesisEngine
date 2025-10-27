#pragma once

#include <functional>
#include <vector>

#include "sandbox/gui/RuntimeBridge.hpp"
#include "sandbox/gui/ui/UiState.hpp"

namespace genesis::sandbox::gui
{

class WorldCommandController
{
public:
    using ToastFn = std::function<void(const std::string&, const ImVec4&, double)>;
    using ResetSceneFn = std::function<void()>;
    using RuntimeProviderFn = std::function<RuntimeBridge*()>;

    WorldCommandController(UiState& state,
                           RuntimeProviderFn runtimeProvider,
                           ToastFn toastFn,
                           ResetSceneFn resetSceneFn);

    void refreshStatuses(const std::vector<RuntimeBridge::CommandProgress>& commands);

private:
    UiState& state_;
    RuntimeProviderFn runtime_provider_;
    ToastFn toast_fn_;
    ResetSceneFn reset_scene_fn_;
};

} // namespace genesis::sandbox::gui

