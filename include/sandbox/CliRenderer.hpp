#pragma once

#include <map>
#include <string>
#include <vector>

#include "genesis/telemetry/TelemetryBuffer.hpp"

namespace sandbox::cli {

struct LayoutNode {
    int x{0};
    int y{0};
    std::string label;
};

struct Layout {
    int width{32};
    int height{12};
    std::map<std::uint32_t, LayoutNode> nodes;
};

struct FrameOptions {
    bool clearScreen{true};
};

class CliRenderer {
public:
    CliRenderer(Layout layout, FrameOptions options = {});
    ~CliRenderer();

    void render(const genesis::telemetry::TickTelemetry& tick);

private:
    Layout m_layout;
    FrameOptions m_options;
    bool m_cursorHidden{false};

    void stamp(std::vector<std::string>& grid, int x, int y, char symbol) const;
    void printSummary(const genesis::telemetry::TickTelemetry& tick) const;
};

} // namespace sandbox::cli
