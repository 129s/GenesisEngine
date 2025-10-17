#include "sandbox/CliApp.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>

namespace {
std::string trim(const std::string& value) {
    auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch); });
    auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}
}

namespace sandbox::cli {

CliApp::CliApp(Layout layout, FrameOptions frameOptions, genesis::runtime::RuntimeConfig config)
    : m_layout(std::move(layout))
    , m_frameOptions(frameOptions)
    , m_runtime(config)
    , m_renderer(m_layout, m_frameOptions) {
    // seed an initial snapshot for rendering
    m_runtime.step(1);
}

int CliApp::run(const std::vector<std::string>& scriptedCommands) {
    auto processAndAdvance = [&](const std::string& command) {
        const bool handled = processInput(command);
        if (!handled && m_running && !m_paused) {
            m_runtime.step(1);
            render();
        }
    };

    if (!scriptedCommands.empty()) {
        for (const auto& command : scriptedCommands) {
            if (!m_running) {
                break;
            }
            processAndAdvance(command);
        }
        return 0;
    }

    std::cout << "Sandbox CLI ready. Commands: [enter=step] step <n>, pause, resume, render, help, quit\n";
    render();

    std::string line;
    while (m_running && std::getline(std::cin, line)) {
        processAndAdvance(line);
    }

    return 0;
}

bool CliApp::processInput(const std::string& line) {
    const std::string trimmed = trim(line);
    if (trimmed.empty()) {
        return false;
    }

    if (trimmed == "quit" || trimmed == "exit") {
        m_running = false;
        return true;
    }

    if (trimmed == "pause") {
        m_paused = true;
        std::cout << "[paused]\n";
        return true;
    }

    if (trimmed == "resume") {
        m_paused = false;
        std::cout << "[resumed]\n";
        return true;
    }

    if (trimmed == "render") {
        render();
        return true;
    }

    if (trimmed == "help" || trimmed == "?") {
        std::cout << "Commands:\n"
                  << "  <enter>        advance one step\n"
                  << "  step <n>       advance n steps\n"
                  << "  pause/resume   toggle simulation\n"
                  << "  render         redraw current snapshot\n"
                  << "  quit/exit      stop CLI\n";
        return true;
    }

    if (trimmed.rfind("step", 0) == 0) {
        std::uint64_t steps = 1;
        std::string arg = trim(trimmed.substr(4));
        if (!arg.empty()) {
            try {
                steps = static_cast<std::uint64_t>(std::stoull(arg));
            } catch (const std::exception&) {
                std::cout << "Invalid step count\n";
                return true;
            }
        }
        m_runtime.step(steps);
        render();
        return true;
    }

    std::cout << "Unknown command: " << trimmed << "\n";
    return true;
}

void CliApp::render() {
    if (const auto* snapshot = m_runtime.latestSnapshot()) {
        m_renderer.render(*snapshot);
    } else {
        std::cout << "(no telemetry yet)\n";
    }
}

} // namespace sandbox::cli
