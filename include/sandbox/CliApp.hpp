#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "genesis/runtime/Runtime.hpp"
#include "sandbox/CliRenderer.hpp"

namespace sandbox::cli {

class CliApp {
public:
    CliApp(Layout layout, FrameOptions frameOptions, genesis::runtime::RuntimeConfig config = {});

    int run(const std::vector<std::string>& scriptedCommands = {});

private:
    bool processInput(const std::string& line, bool allowSleep);
    void enforceFrameRate(bool allowSleep);
    std::chrono::steady_clock::duration desiredFrameInterval() const;
    void render();

    Layout m_layout;
    FrameOptions m_frameOptions;
    genesis::runtime::Runtime m_runtime;
    CliRenderer m_renderer;
    bool m_running{true};
    bool m_paused{false};
    std::chrono::steady_clock::time_point m_lastFrameTime{};
};

} // namespace sandbox::cli
