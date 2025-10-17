#include "sandbox/CliRenderer.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

namespace sandbox::cli {

CliRenderer::CliRenderer(Layout layout, FrameOptions options)
    : m_layout(std::move(layout))
    , m_options(options) {
}

CliRenderer::~CliRenderer() {
    if (m_cursorHidden && m_options.clearScreen) {
        std::cout << "\x1b[?25h" << std::flush;
    }
}

void CliRenderer::stamp(std::vector<std::string>& grid, int x, int y, char symbol) const {
    if (y < 0 || y >= static_cast<int>(grid.size())) {
        return;
    }
    if (x < 0 || x >= static_cast<int>(grid[y].size())) {
        return;
    }
    char& cell = grid[y][x];
    if (cell == '.' || cell == symbol) {
        cell = symbol;
    } else {
        cell = '#';
    }
}

void CliRenderer::printSummary(const genesis::telemetry::TickTelemetry& tick, std::ostream& out) const {
    const auto previousFlags = out.flags();
    const auto previousPrecision = out.precision();

    out << "Legend: A=Agent M=MoveTo C=ConsumeResource F=Food #=collision" << "\n";

    if (!tick.actions.empty()) {
        out << "Actions:\n";
        for (const auto& action : tick.actions) {
            out << "  #" << action.entityId
                << " action=" << action.currentAction
                << " queue=" << action.queueLength
                << " target=" << action.target.value
                << " speed=" << action.speed
                << "\n";
        }
    }

    if (!tick.needs.empty()) {
        out << "Needs:\n";
        for (const auto& need : tick.needs) {
            out << "  #" << need.entityId
                << " " << need.needName
                << "=" << std::fixed << std::setprecision(1) << need.value
                << (need.critical ? " !" : "") << "\n";
        }
    }

    out.flags(previousFlags);
    out.precision(previousPrecision);
}

void CliRenderer::render(const genesis::telemetry::TickTelemetry& tick) {
    std::vector<std::string> grid(m_layout.height, std::string(m_layout.width, '.'));

    auto stampNode = [&](genesis::world::LocationId location, char symbol) {
        auto it = m_layout.nodes.find(location.value);
        if (it != m_layout.nodes.end()) {
            stamp(grid, it->second.x, it->second.y, symbol);
        }
    };

    for (const auto& resource : tick.resources) {
        const char symbol = resource.type == genesis::world::ResourceType::Food ? 'F' : 'R';
        stampNode(resource.location, symbol);
    }

    for (const auto& action : tick.actions) {
        const char symbol = action.currentAction == "ConsumeResource" ? 'C' : 'M';
        stampNode(action.target, symbol);
    }

    for (const auto& agent : tick.agents) {
        stampNode(agent.location, 'A');
    }

    std::ostringstream summary;
    printSummary(tick, summary);

    std::ostringstream frame;

    if (m_options.clearScreen) {
        if (!m_cursorHidden) {
            frame << "\x1b[?25l";
            m_cursorHidden = true;
        }
        frame << "\x1b[H";
    } else {
        frame << "\n";
    }

    frame << "Step " << tick.step << "\n";
    for (const auto& row : grid) {
        frame << row << "\n";
    }

    frame << summary.str();

    if (m_options.clearScreen) {
        frame << "\x1b[J";
    }

    std::cout << frame.str();
    std::cout.flush();
}

} // namespace sandbox::cli




