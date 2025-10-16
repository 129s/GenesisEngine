#include <chrono>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#    include <windows.h>
#endif

namespace {

struct LayoutNode {
    int x{0};
    int y{0};
    std::string label;
};

struct Layout {
    int width{32};
    int height{12};
    std::unordered_map<std::uint32_t, LayoutNode> nodes;
};

struct CliOptions {
    std::string telemetryFile;
    std::string layoutFile{"data/ascii_layout.json"};
    int delayMs{150};
    int maxFrames{-1};
    bool clearScreen{true};
};

void printUsage() {
    std::cout << "Usage: genesis_ascii_viewer --telemetry=<file> [--layout=<file>] [--delay=<ms>] [--frames=<count>] [--no-clear]\n"
              << "Renders telemetry playback as ASCII frames.\n";
}

std::optional<CliOptions> parseOptions(int argc, char** argv) {
    CliOptions options;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            printUsage();
            return std::nullopt;
        }
        if (arg.rfind("--telemetry=", 0) == 0) {
            options.telemetryFile = std::string(arg.substr(12));
        } else if (arg == "--telemetry") {
            if (i + 1 >= argc) {
                spdlog::error("--telemetry requires a file path");
                return std::nullopt;
            }
            options.telemetryFile = argv[++i];
        } else if (arg.rfind("--layout=", 0) == 0) {
            options.layoutFile = std::string(arg.substr(9));
        } else if (arg == "--layout") {
            if (i + 1 >= argc) {
                spdlog::error("--layout requires a file path");
                return std::nullopt;
            }
            options.layoutFile = argv[++i];
        } else if (arg.rfind("--delay=", 0) == 0) {
            options.delayMs = std::stoi(std::string(arg.substr(8)));
        } else if (arg == "--delay") {
            if (i + 1 >= argc) {
                spdlog::error("--delay requires a value");
                return std::nullopt;
            }
            options.delayMs = std::stoi(argv[++i]);
        } else if (arg.rfind("--frames=", 0) == 0) {
            options.maxFrames = std::stoi(std::string(arg.substr(9)));
        } else if (arg == "--frames") {
            if (i + 1 >= argc) {
                spdlog::error("--frames requires a value");
                return std::nullopt;
            }
            options.maxFrames = std::stoi(argv[++i]);
        } else if (arg == "--no-clear") {
            options.clearScreen = false;
        } else {
            spdlog::error("Unknown argument {}", arg);
            return std::nullopt;
        }
    }

    if (options.telemetryFile.empty()) {
        spdlog::error("Missing --telemetry=<file> argument");
        return std::nullopt;
    }

    return options;
}

Layout loadLayout(const std::string& path) {
    Layout layout;

    std::ifstream stream(path);
    if (!stream) {
        spdlog::warn("Could not open layout file {}, using defaults", path);
        return layout;
    }

    nlohmann::json json;
    stream >> json;

    layout.width = json.value("width", layout.width);
    layout.height = json.value("height", layout.height);

    if (json.contains("nodes")) {
        for (const auto& node : json["nodes"]) {
            LayoutNode info{};
            const auto id = node.value("id", 0);
            info.x = node.value("x", 0);
            info.y = node.value("y", 0);
            info.label = node.value("label", "");
            layout.nodes.emplace(static_cast<std::uint32_t>(id), std::move(info));
        }
    }

    return layout;
}

nlohmann::json loadTelemetry(const std::string& path) {
    std::ifstream stream(path);
    if (!stream) {
        spdlog::error("Failed to open telemetry file {}", path);
        return {};
    }

    nlohmann::json json;
    stream >> json;
    return json;
}

char resourceSymbol(int type) {
    switch (type) {
    case 0:
        return 'F';
    case 1:
        return 'D';
    case 2:
        return 'S';
    default:
        return 'R';
    }
}

struct Position {
    int x{0};
    int y{0};
    bool valid{false};
};

Position locate(const Layout& layout, std::uint32_t locationId) {
    if (auto it = layout.nodes.find(locationId); it != layout.nodes.end()) {
        return {it->second.x, it->second.y, true};
    }
    return {};
}

void renderFrame(const nlohmann::json& tick, const Layout& layout, std::size_t frameIndex, std::size_t totalFrames) {
    std::vector<std::string> grid(layout.height, std::string(layout.width, '.'));

    auto stamp = [&](const Position& pos, char symbol) {
        if (!pos.valid) {
            return;
        }
        if (pos.y < 0 || pos.y >= layout.height || pos.x < 0 || pos.x >= layout.width) {
            return;
        }
        char& cell = grid[pos.y][pos.x];
        if (cell == '.' || cell == symbol) {
            cell = symbol;
        } else {
            cell = '#';
        }
    };

    if (tick.contains("resources")) {
        for (const auto& res : tick["resources"]) {
            const auto location = static_cast<std::uint32_t>(res.value("location", 0));
            const auto type = res.value("type", 0);
            const Position pos = locate(layout, location);
            stamp(pos, resourceSymbol(type));
        }
    }

    if (tick.contains("actions")) {
        for (const auto& action : tick["actions"]) {
            const auto location = static_cast<std::uint32_t>(action.value("target", 0));
            const std::string currentAction = action.value("currentAction", "");
            char symbol = currentAction == "ConsumeResource" ? 'C' : 'M';
            const Position pos = locate(layout, location);
            stamp(pos, symbol);
        }
    }

    if (tick.contains("agents")) {
        for (const auto& agent : tick["agents"]) {
            const auto location = static_cast<std::uint32_t>(agent.value("location", 0));
            const Position pos = locate(layout, location);
            stamp(pos, 'A');
        }
    }

    std::cout << "Frame " << (frameIndex + 1) << "/" << totalFrames << " | Step " << tick.value("step", 0) << "\n";
    for (const auto& row : grid) {
        std::cout << row << "\n";
    }

    std::unordered_map<std::uint32_t, const nlohmann::json*> actionByEntity;
    if (tick.contains("actions")) {
        for (const auto& action : tick["actions"]) {
            actionByEntity.emplace(static_cast<std::uint32_t>(action.value("entityId", 0)), &action);
        }
    }

    std::unordered_map<std::uint32_t, const nlohmann::json*> needByEntity;
    if (tick.contains("needs")) {
        for (const auto& need : tick["needs"]) {
            needByEntity.emplace(static_cast<std::uint32_t>(need.value("entityId", 0)), &need);
        }
    }

    if (tick.contains("agents")) {
        std::cout << "Agents:\n";
        for (const auto& agent : tick["agents"]) {
            const auto id = static_cast<std::uint32_t>(agent.value("entityId", 0));
            const auto locationId = static_cast<std::uint32_t>(agent.value("location", 0));
            std::string locationLabel = "<unknown>";
            if (auto it = layout.nodes.find(locationId); it != layout.nodes.end()) {
                locationLabel = it->second.label;
            }

            std::cout << "  #" << id << " @ " << locationLabel << " (loc=" << locationId << ")";

            if (auto it = actionByEntity.find(id); it != actionByEntity.end()) {
                const auto& action = *it->second;
                std::cout << " | action=" << action.value("currentAction", std::string{})
                          << " queue=" << action.value("queueLength", 0);
            }

            if (auto it = needByEntity.find(id); it != needByEntity.end()) {
                const auto& need = *it->second;
                std::cout << " | " << need.value("needName", std::string{}) << "=" << need.value("value", 0.0);
            }

            std::cout << "\n";
        }
    }
}

} // namespace

#ifdef _WIN32
bool enableVirtualTerminal() {
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD mode = 0;
    if (!GetConsoleMode(handle, &mode)) {
        return false;
    }

    if (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) {
        return true;
    }

    DWORD newMode = mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(handle, newMode) != 0;
}
#else
bool enableVirtualTerminal() {
    return true;
}
#endif

int main(int argc, char** argv) {
    auto options = parseOptions(argc, argv);
    if (!options) {
        return 1;
    }

    bool vtSupported = enableVirtualTerminal();
    if (!vtSupported && options->clearScreen) {
        spdlog::warn("Terminal does not support ANSI escape codes, disabling screen clearing");
        options->clearScreen = false;
    }

    const Layout layout = loadLayout(options->layoutFile);
    const nlohmann::json telemetry = loadTelemetry(options->telemetryFile);
    if (telemetry.empty() || !telemetry.contains("ticks")) {
        spdlog::error("Telemetry data is empty or malformed");
        return 1;
    }

    const auto& ticks = telemetry.at("ticks");
    const std::size_t totalFrames = ticks.size();
    const std::size_t framesToShow = options->maxFrames < 0
        ? totalFrames
        : std::min<std::size_t>(static_cast<std::size_t>(options->maxFrames), totalFrames);

    bool cursorHidden = false;
    for (std::size_t idx = 0; idx < framesToShow; ++idx) {
        if (options->clearScreen) {
            if (!cursorHidden) {
                std::cout << "\x1b[?25l";
                cursorHidden = true;
            }
            std::cout << "\x1b[H\x1b[2J";
        } else if (idx > 0) {
            std::cout << "\n";
        }
        renderFrame(ticks.at(idx), layout, idx, totalFrames);
        std::cout.flush();
        if (options->delayMs > 0 && idx + 1 < framesToShow) {
            std::this_thread::sleep_for(std::chrono::milliseconds(options->delayMs));
        }
    }

    if (cursorHidden) {
        std::cout << "\x1b[?25h" << std::flush;
    }

    return 0;
}
