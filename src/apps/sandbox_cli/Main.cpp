#include "sandbox/CliApp.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include <nlohmann/json.hpp>
#include "genesis/world/generation/NoiseGridGenerator.hpp"

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
    bool autoRun{false};
    bool generateNoise{false};
    std::uint64_t noiseSeed{1337};
    std::uint32_t noiseWidth{64};
    std::uint32_t noiseHeight{64};
    double noiseThreshold{0.5};
    double noiseDensity{0.05};
    std::uint32_t noiseCapacity{24};
    std::uint32_t noiseRate{3};
};

class EnvVarGuard {
public:
    explicit EnvVarGuard(std::string name)
        : m_name(std::move(name)) {
        if (const char* value = std::getenv(m_name.c_str())) {
            m_previous = value;
            m_hadPrevious = true;
        }
    }

    void set(const std::string& value) {
#ifdef _WIN32
        _putenv_s(m_name.c_str(), value.c_str());
#else
        ::setenv(m_name.c_str(), value.c_str(), 1);
#endif
        m_active = true;
    }

    ~EnvVarGuard() {
        if (!m_active) {
            return;
        }
#ifdef _WIN32
        if (m_hadPrevious) {
            _putenv_s(m_name.c_str(), m_previous.c_str());
        } else {
            _putenv_s(m_name.c_str(), "");
        }
#else
        if (m_hadPrevious) {
            ::setenv(m_name.c_str(), m_previous.c_str(), 1);
        } else {
            ::unsetenv(m_name.c_str());
        }
#endif
    }

private:
    std::string m_name;
    std::string m_previous;
    bool m_hadPrevious{false};
    bool m_active{false};
};

class TempFileGuard {
public:
    ~TempFileGuard() {
        for (const auto& path : m_paths) {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    }

    void add(std::filesystem::path path) {
        m_paths.push_back(std::move(path));
    }

private:
    std::vector<std::filesystem::path> m_paths;
};

sandbox::cli::Layout layoutFromGeneration(const genesis::world::generation::GenerationLayout& layoutData) {
    sandbox::cli::Layout layout;
    layout.width = static_cast<int>(layoutData.width);
    layout.height = static_cast<int>(layoutData.height);
    for (const auto& node : layoutData.nodes) {
        sandbox::cli::LayoutNode info{};
        info.x = node.x;
        info.y = node.y;
        info.label = node.label;
        layout.nodes.emplace(node.id.value, std::move(info));
    }
    return layout;
}

std::filesystem::path makeTempPath(const std::string& prefix, const std::string& extension) {
    auto base = prefix + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    return std::filesystem::temp_directory_path() / (base + extension);
}

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
        } else if (arg == "--auto-run") {
            options.autoRun = true;
        } else if (arg == "--generate-noise") {
            options.generateNoise = true;
        } else if (arg.rfind("--noise-seed=", 0) == 0) {
            try {
                options.noiseSeed = std::stoull(arg.substr(13));
            } catch (const std::exception&) {
                std::cerr << "Invalid noise seed value: " << arg.substr(13) << "\n";
                options.showHelp = true;
            }
        } else if (arg == "--noise-seed" && i + 1 < argc) {
            try {
                options.noiseSeed = std::stoull(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "Invalid noise seed value: " << argv[i] << "\n";
                options.showHelp = true;
            }
        } else if (arg.rfind("--noise-width=", 0) == 0) {
            try {
                options.noiseWidth = static_cast<std::uint32_t>(std::stoul(arg.substr(13)));
            } catch (const std::exception&) {
                std::cerr << "Invalid noise width value: " << arg.substr(13) << "\n";
                options.showHelp = true;
            }
        } else if (arg == "--noise-width" && i + 1 < argc) {
            try {
                options.noiseWidth = static_cast<std::uint32_t>(std::stoul(argv[++i]));
            } catch (const std::exception&) {
                std::cerr << "Invalid noise width value: " << argv[i] << "\n";
                options.showHelp = true;
            }
        } else if (arg.rfind("--noise-height=", 0) == 0) {
            try {
                options.noiseHeight = static_cast<std::uint32_t>(std::stoul(arg.substr(14)));
            } catch (const std::exception&) {
                std::cerr << "Invalid noise height value: " << arg.substr(14) << "\n";
                options.showHelp = true;
            }
        } else if (arg == "--noise-height" && i + 1 < argc) {
            try {
                options.noiseHeight = static_cast<std::uint32_t>(std::stoul(argv[++i]));
            } catch (const std::exception&) {
                std::cerr << "Invalid noise height value: " << argv[i] << "\n";
                options.showHelp = true;
            }
        } else if (arg.rfind("--noise-threshold=", 0) == 0) {
            try {
                options.noiseThreshold = std::stod(arg.substr(17));
            } catch (const std::exception&) {
                std::cerr << "Invalid noise threshold value: " << arg.substr(17) << "\n";
                options.showHelp = true;
            }
        } else if (arg == "--noise-threshold" && i + 1 < argc) {
            try {
                options.noiseThreshold = std::stod(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "Invalid noise threshold value: " << argv[i] << "\n";
                options.showHelp = true;
            }
        } else if (arg.rfind("--noise-density=", 0) == 0) {
            try {
                options.noiseDensity = std::stod(arg.substr(15));
            } catch (const std::exception&) {
                std::cerr << "Invalid noise density value: " << arg.substr(15) << "\n";
                options.showHelp = true;
            }
        } else if (arg == "--noise-density" && i + 1 < argc) {
            try {
                options.noiseDensity = std::stod(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "Invalid noise density value: " << argv[i] << "\n";
                options.showHelp = true;
            }
        } else if (arg.rfind("--noise-capacity=", 0) == 0) {
            try {
                options.noiseCapacity = static_cast<std::uint32_t>(std::stoul(arg.substr(16)));
            } catch (const std::exception&) {
                std::cerr << "Invalid noise capacity value: " << arg.substr(16) << "\n";
                options.showHelp = true;
            }
        } else if (arg == "--noise-capacity" && i + 1 < argc) {
            try {
                options.noiseCapacity = static_cast<std::uint32_t>(std::stoul(argv[++i]));
            } catch (const std::exception&) {
                std::cerr << "Invalid noise capacity value: " << argv[i] << "\n";
                options.showHelp = true;
            }
        } else if (arg.rfind("--noise-rate=", 0) == 0) {
            try {
                options.noiseRate = static_cast<std::uint32_t>(std::stoul(arg.substr(12)));
            } catch (const std::exception&) {
                std::cerr << "Invalid noise rate value: " << arg.substr(12) << "\n";
                options.showHelp = true;
            }
        } else if (arg == "--noise-rate" && i + 1 < argc) {
            try {
                options.noiseRate = static_cast<std::uint32_t>(std::stoul(argv[++i]));
            } catch (const std::exception&) {
                std::cerr << "Invalid noise rate value: " << argv[i] << "\n";
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
              << "  --auto-run          continuously advance simulation until quit\n"
              << "  --generate-noise    generate a noise world for this session\n"
              << "  --noise-seed <n>    seed for generated noise world (default 1337)\n"
              << "  --noise-width <n>   noise grid width (default 64)\n"
              << "  --noise-height <n>  noise grid height (default 64)\n"
              << "  --noise-threshold <0-1> noise threshold (default 0.5)\n"
              << "  --noise-density <0-1>   soil spawn density (default 0.05)\n"
              << "  --noise-capacity <n>    resource capacity per spawn (default 24)\n"
              << "  --noise-rate <n>        resource rate per step (default 3)\n"
              << "  --no-clear          disable ANSI screen clearing\n"
              << "  --help              show this message\n";
}

} // namespace

int main(int argc, char** argv) {
    auto options = parseArgs(argc, argv);

    if (!options.showHelp && options.generateNoise) {
        if (options.noiseWidth == 0U || options.noiseHeight == 0U) {
            std::cerr << "Noise width/height must be greater than zero.\n";
            options.showHelp = true;
        }
        if (options.noiseThreshold < 0.0 || options.noiseThreshold > 1.0) {
            std::cerr << "Noise threshold must be within [0,1].\n";
            options.showHelp = true;
        }
        if (options.noiseDensity < 0.0 || options.noiseDensity > 1.0) {
            std::cerr << "Noise density must be within [0,1].\n";
            options.showHelp = true;
        }
    }

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

    EnvVarGuard envGuard("GENESIS_WORLD_PATH");
    TempFileGuard tempFiles;

    sandbox::cli::Layout layout{};
    bool layoutInitialized = false;

    if (options.generateNoise) {
        genesis::world::generation::NoiseGridConfig config;
        config.width = options.noiseWidth;
        config.height = options.noiseHeight;
        config.threshold = options.noiseThreshold;
        config.soilSpawnDensity = options.noiseDensity;
        config.resourceCapacity = options.noiseCapacity;
        config.resourceRatePerStep = options.noiseRate;

        genesis::world::generation::NoiseGridGenerator generator;
        const auto result = generator.generate(options.noiseSeed, config);

        const auto worldPath = makeTempPath("genesis_noise_world_", ".json");
        const auto layoutPathTemp = makeTempPath("genesis_noise_layout_", ".json");
        genesis::world::generation::writeNoiseGenerationOutputs(result, worldPath, layoutPathTemp);
        tempFiles.add(worldPath);
        tempFiles.add(layoutPathTemp);
        envGuard.set(worldPath.string());

        layout = layoutFromGeneration(result.layout);
        layoutInitialized = true;
    }

    if (!layoutInitialized) {
        std::string layoutPath = options.layoutPath;
        if (layoutPath.empty()) {
            if (const auto located = findDataFile("data/ascii_layout.json"); !located.empty()) {
                layoutPath = located.string();
            }
        }
        layout = loadLayout(layoutPath);
    }

    if (!enableAnsiSequences() && frameOptions.clearScreen) {
        std::cerr << "Terminal does not support ANSI escape sequences; disabling clear screen.\n";
        frameOptions.clearScreen = false;
    }

    sandbox::cli::CliApp app(std::move(layout), frameOptions, options.autoRun, {});
    return app.run(options.commands);
}
