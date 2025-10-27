#include "sandbox/gui/ui/UiState.hpp"

#include <imgui.h>

#include <utility>

namespace genesis::sandbox::gui
{

void UiState::pushToast(const std::string& text, const ImVec4& color, double lifetime_sec)
{
    Toast toast;
    toast.text = text;
    toast.color = color;
    toast.expires_at = ImGui::GetTime() + lifetime_sec;
    toasts.emplace_back(std::move(toast));
    if (toasts.size() > 8)
    {
        toasts.pop_front();
    }
}

void UiState::pruneExpiredToasts(double now)
{
    while (!toasts.empty() && toasts.front().expires_at <= now)
    {
        toasts.pop_front();
    }
}

} // namespace genesis::sandbox::gui
