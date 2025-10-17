#include "sandbox/CliApp.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#    include <windows.h>
#endif

namespace {

struct Options {
    std::string layoutPath;
    bool noClear{false};
    std::vector<std::string> commands;
    bool showHelp{false};
    std::optional<int> fps;
    std::optional<int> frameTimeMs;
};

std::string trim(const std::string& value) {
    auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch); });
    auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

std::vector<std::string> splitCommands(const std::string& script) {
    std::vector<std::string> result;
    std::stringstream ss(script);
    std::string segment;
    while (std::getline(ss, segment, ';')) {
        auto trimmed = trim(segment);
        if (!trimmed.empty()) {
            result.push_back(std::move(trimmed));
        }
    }
    return result;
}

Options parseArgs(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            options.showHelp = true;
            return options;
        } else if (arg == "--no-clear") {
            options.noClear = true;
        } else if (arg.rfind("--layout=", 0) == 0) {
            options.layoutPath = arg.substr(9);
        } else if (arg == "--layout" && i + 1 < argc) {
            options.layoutPath = argv[++i];
        } else if (arg.rfind("--commands=", 0) == 0) {
            auto more = splitCommands(arg.substr(11));
            options.commands.insert(options.commands.end(), more.begin(), more.end());
        } else if (arg == "--commands" && i + 1 < argc) {
            auto more = splitCommands(argv[++i]);
            options.commands.insert(options.commands.end(), more.begin(), more.end());
        } else if (arg.rfind("--fps=", 0) == 0) {
            try {
                options.fps = std::stoi(arg.substr(6));
            } catch (const std::exception&) {
                std::cerr << "Invalid fps value: " << arg.substr(6) << "\n";
                options.showHelp = true;
            }
        } else if (arg == "--fps" && i + 1 < argc) {
            try {
                options.fps = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "Invalid fps value: " << argv[i] << "\n";
                options.showHelp = true;
            }
        } else if (arg.rfind("--frame-time-ms=", 0) == 0) {
            try {
                options.frameTimeMs = std::stoi(arg.substr(15));
            } catch (const std::exception&) {
                std::cerr << "Invalid frame time value: " << arg.substr(15) << "\n";
                options.showHelp = true;
            }
        } else if (arg == "--frame-time-ms" && i + 1 < argc) {
            try {
                options.frameTimeMs = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "Invalid frame time value: " << argv[i] << "\n";
                options.showHelp = true;
            }
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            options.showHelp = true;
        }
    }
    return options;
}

std::filesystem::path findDataFile(const std::filesystem::path& relative) {
    constexpr int searchDepth = 4;
    auto current = std::filesystem::current_path();
    for (int i = 0; i < searchDepth; ++i) {
        const auto candidate = current / relative;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
        if (current.has_parent_path()) {
            current = current.parent_path();
        } else {
            break;
        }
    }
    return {};
}

sandbox::cli::Layout defaultLayout() {
    sandbox::cli::Layout layout;
    layout.width = 25;
    layout.height = 11;
    layout.nodes.emplace(1, sandbox::cli::LayoutNode{12, 5, "Town Center"});
    layout.nodes.emplace(2, sandbox::cli::LayoutNode{16, 5, "Tavern"});
    layout.nodes.emplace(3, sandbox::cli::LayoutNode{18, 4, "Kitchen"});
    layout.nodes.emplace(4, sandbox::cli::LayoutNode{18, 6, "Common Hall"});
    layout.nodes.emplace(5, sandbox::cli::LayoutNode{8, 5, "Residential"});
    layout.nodes.emplace(6, sandbox::cli::LayoutNode{6, 4, "Dormitory"});
    return layout;
}

sandbox::cli::Layout loadLayout(const std::string& path) {
    sandbox::cli::Layout layout = defaultLayout();

    if (path.empty()) {
        return layout;
    }

    std::ifstream stream(path);
    if (!stream) {
        std::cerr << "Failed to open layout file: " << path << ". Using defaults.\n";
        return layout;
    }

    try {
        nlohmann::json json;
        stream >> json;
        layout.width = json.value("width", layout.width);
        layout.height = json.value("height", layout.height);
        if (json.contains("nodes")) {
            layout.nodes.clear();
            for (const auto& node : json["nodes"]) {
                const auto id = node.value("id", 0);
                sandbox::cli::LayoutNode info{};
                info.x = node.value("x", 0);
                info.y = node.value("y", 0);
                info.label = node.value("label", "");
                layout.nodes.emplace(static_cast<std::uint32_t>(id), std::move(info));
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "Failed to parse layout file: " << ex.what() << ". Using defaults.\n";
        layout = defaultLayout();
    }

    return layout;
}

bool enableAnsiSequences() {
#ifdef _WIN32
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
    DWORD request = mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(handle, request) != 0;
#else
    return true;
#endif
}

void printUsage() {
    std::cout << "sandbox-cli usage:\n"
              << "  --layout <path>     layout JSON (defaults to data/ascii_layout.json)\n"
              << "  --commands <list>   semicolon-separated command script (non-interactive)\n"
              << "  --fps <value>       target render frame rate (0 disables limit, default 60)\n"
              << "  --frame-time-ms <n> override minimum frame time in milliseconds\n"
              << "  --no-clear          disable ANSI screen clearing\n"
              << "  --help              show this message\n";
}

} // namespace

int main(int argc, char** argv) {
    const auto options = parseArgs(argc, argv);
    if (options.showHelp) {
        printUsage();
        return 0;
    }

    sandbox::cli::FrameOptions frameOptions{};
    frameOptions.clearScreen = !options.noClear;
    if (options.frameTimeMs) {
        if (*options.frameTimeMs <= 0) {
            frameOptions.limitFrameRate = false;
        } else {
            frameOptions.limitFrameRate = true;
            frameOptions.minFrameTime = std::chrono::milliseconds(*options.frameTimeMs);
        }
    } else if (options.fps) {
        if (*options.fps <= 0) {
            frameOptions.limitFrameRate = false;
        } else {
            const auto frameMs = std::max(1, static_cast<int>(std::lround(1000.0 / static_cast<double>(*options.fps))));
            frameOptions.limitFrameRate = true;
            frameOptions.minFrameTime = std::chrono::milliseconds(frameMs);
        }
    }

    std::string layoutPath = options.layoutPath;
    if (layoutPath.empty()) {
        if (const auto located = findDataFile("data/ascii_layout.json"); !located.empty()) {
            layoutPath = located.string();
        }
    }

    auto layout = loadLayout(layoutPath);

    if (!enableAnsiSequences() && frameOptions.clearScreen) {
        std::cerr << "Terminal does not support ANSI escape sequences; disabling clear screen.\n";
        frameOptions.clearScreen = false;
    }

    sandbox::cli::CliApp app(std::move(layout), frameOptions, {});
    return app.run(options.commands);
}
