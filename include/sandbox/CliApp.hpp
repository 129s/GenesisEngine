#pragma once

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
    bool processInput(const std::string& line);
    void render();

    Layout m_layout;
    FrameOptions m_frameOptions;
    genesis::runtime::Runtime m_runtime;
    CliRenderer m_renderer;
    bool m_running{true};
    bool m_paused{false};
};

} // namespace sandbox::cli
