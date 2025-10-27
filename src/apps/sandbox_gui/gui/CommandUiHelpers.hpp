#pragma once

#include <string>

#include <imgui.h>

#include "sandbox/gui/RuntimeBridge.hpp"
#include "sandbox/gui/style/DesignTokens.hpp"

namespace genesis::sandbox::gui
{

inline const char* commandStateLabel(RuntimeBridge::CommandState state)
{
    switch (state)
    {
    case RuntimeBridge::CommandState::Pending:
        return "Pending";
    case RuntimeBridge::CommandState::Succeeded:
        return "Succeeded";
    case RuntimeBridge::CommandState::Failed:
        return "Failed";
    default:
        return "Unknown";
    }
}

inline ImVec4 commandStateColor(RuntimeBridge::CommandState state)
{
    switch (state)
    {
    case RuntimeBridge::CommandState::Pending:
        return Style::DesignTokens::color(Style::ColorToken::Warning);
    case RuntimeBridge::CommandState::Succeeded:
        return Style::DesignTokens::color(Style::ColorToken::Success);
    case RuntimeBridge::CommandState::Failed:
        return Style::DesignTokens::color(Style::ColorToken::Danger);
    default:
        return Style::DesignTokens::color(Style::ColorToken::Muted);
    }
}

inline std::string commandStateSummary(const RuntimeBridge::CommandProgress& command)
{
    std::string summary = std::string(commandStateLabel(command.state)) + " (#" + std::to_string(command.id) + ")";
    if (!command.message.empty())
    {
        summary += " · " + command.message;
    }
    return summary;
}

} // namespace genesis::sandbox::gui
