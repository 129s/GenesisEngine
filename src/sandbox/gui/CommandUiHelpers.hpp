#pragma once

#include <string>

#include <imgui.h>

#include "sandbox/gui/RuntimeBridge.hpp"

namespace Genesis::Sandbox::Gui
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
        return ImVec4(0.95f, 0.78f, 0.35f, 1.0f);
    case RuntimeBridge::CommandState::Succeeded:
        return ImVec4(0.45f, 0.85f, 0.45f, 1.0f);
    case RuntimeBridge::CommandState::Failed:
        return ImVec4(0.95f, 0.4f, 0.35f, 1.0f);
    default:
        return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
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

} // namespace Genesis::Sandbox::Gui

