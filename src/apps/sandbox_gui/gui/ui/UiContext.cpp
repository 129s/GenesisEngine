#include "sandbox/gui/ui/UiContext.hpp"

#include "sandbox/gui/AppHost.hpp"
#include "sandbox/gui/controller/WorldCommandController.hpp"

namespace Genesis::Sandbox::Gui
{

void UiContext::pushToast(const std::string& text, const ImVec4& color, double lifetime_sec)
{
    state.pushToast(text, color, lifetime_sec);
}

void UiContext::resetSceneForNewWorld()
{
    host.resetSceneForNewWorld();
}

void UiContext::resetMapViewCamera()
{
    host.resetMapViewCamera();
}

void UiContext::updateWorldCommandStatuses(const std::vector<RuntimeBridge::CommandProgress>& commands)
{
    world_commands.refreshStatuses(commands);
}

} // namespace Genesis::Sandbox::Gui
