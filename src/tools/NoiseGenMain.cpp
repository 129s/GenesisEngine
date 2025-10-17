#include "genesis/world/generation/NoiseGridGenerator.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using genesis::world::generation::NoiseGenerationResult;
using genesis::world::generation::NoiseGridConfig;
using genesis::world::generation::NoiseGridGenerator;
using genesis::world::generation::writeNoiseGenerationOutputs;

struct ParsedArgs {
    std::uint64_t seed{0};
    std::optional<std::uint32_t> width;
    std::optional<std::uint32_t> height;
    std::optional<double> threshold;
    std::optional<double> soilDensity;
    std::optional<std::uint32_t> capacity;
    std::optional<std::uint32_t> ratePerStep;
    std::filesystem::path worldPath{"data/world/generated/noise_mvp.json"};
    std::filesystem::path layoutPath{"data/world/generated/noise_mvp_layout.json"};
    bool help{false};
};

template <typename T>
std::optional<T> parseValue(std::string_view value);

template <>
std::optional<std::uint32_t> parseValue<std::uint32_t>(std::string_view value) {
    std::uint32_t output{};
    const auto result = std::from_chars(value.data(), value.data() + value.size(), output);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        return std::nullopt;
    }
    return output;
}

template <>
std::optional<std::uint64_t> parseValue<std::uint64_t>(std::string_view value) {
    std::uint64_t output{};
    const auto result = std::from_chars(value.data(), value.data() + value.size(), output);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        return std::nullopt;
    }
    return output;
}

template <>
std::optional<double> parseValue<double>(std::string_view value) {
    try {
#if defined(_MSC_VER)
        return std::stod(std::string{value});
#else
        std::size_t idx{};
        const double parsed = std::stod(std::string{value}, &idx);
        if (idx != value.size()) {
            return std::nullopt;
        }
        return parsed;
#endif
    } catch (...) {
        return std::nullopt;
    }
}

void printUsage() {
    std::cout << "Usage: genesis_noise_generator [options]\n\n"
              << "Options:\n"
              << "  --seed=<value>            Seed for deterministic generation (required)\n"
              << "  --width=<value>           Grid width (default 64)\n"
              << "  --height=<value>          Grid height (default 64)\n"
              << "  --threshold=<value>       Noise threshold separating soil/stone (default 0.5)\n"
              << "  --soil-density=<value>    Probability for soil spawn placement (default 0.05)\n"
              << "  --capacity=<value>        Food spawn capacity (default 24)\n"
              << "  --rate=<value>            Food spawn rate per step (default 3)\n"
              << "  --world=<path>            Output world JSON path\n"
              << "  --layout=<path>           Output layout JSON path\n"
              << "  --help                    Show this message\n";
}

std::optional<ParsedArgs> parseArguments(const std::vector<std::string>& args) {
    ParsedArgs parsed;

    for (const auto& arg : args) {
        if (arg == "--help" || arg == "-h") {
            parsed.help = true;
            continue;
        }

        const auto equalPos = arg.find('=');
        if (equalPos == std::string::npos) {
            std::cerr << "Unrecognized argument (expected key=value): " << arg << '\n';
            return std::nullopt;
        }

        const std::string key = arg.substr(0, equalPos);
        const std::string_view value(arg.data() + equalPos + 1, arg.size() - equalPos - 1);

        if (key == "--seed") {
            auto parsedSeed = parseValue<std::uint64_t>(value);
            if (!parsedSeed) {
                std::cerr << "Invalid seed value: " << value << '\n';
                return std::nullopt;
            }
            parsed.seed = *parsedSeed;
        } else if (key == "--width") {
            auto parsedWidth = parseValue<std::uint32_t>(value);
            if (!parsedWidth) {
                std::cerr << "Invalid width value: " << value << '\n';
                return std::nullopt;
            }
            parsed.width = *parsedWidth;
        } else if (key == "--height") {
            auto parsedHeight = parseValue<std::uint32_t>(value);
            if (!parsedHeight) {
                std::cerr << "Invalid height value: " << value << '\n';
                return std::nullopt;
            }
            parsed.height = *parsedHeight;
        } else if (key == "--threshold") {
            auto parsedThreshold = parseValue<double>(value);
            if (!parsedThreshold || *parsedThreshold < 0.0 || *parsedThreshold > 1.0) {
                std::cerr << "Invalid threshold value: " << value << '\n';
                return std::nullopt;
            }
            parsed.threshold = *parsedThreshold;
        } else if (key == "--soil-density") {
            auto parsedDensity = parseValue<double>(value);
            if (!parsedDensity || *parsedDensity < 0.0 || *parsedDensity > 1.0) {
                std::cerr << "Invalid soil-density value: " << value << '\n';
                return std::nullopt;
            }
            parsed.soilDensity = *parsedDensity;
        } else if (key == "--capacity") {
            auto parsedCapacity = parseValue<std::uint32_t>(value);
            if (!parsedCapacity) {
                std::cerr << "Invalid capacity value: " << value << '\n';
                return std::nullopt;
            }
            parsed.capacity = *parsedCapacity;
        } else if (key == "--rate") {
            auto parsedRate = parseValue<std::uint32_t>(value);
            if (!parsedRate) {
                std::cerr << "Invalid rate value: " << value << '\n';
                return std::nullopt;
            }
            parsed.ratePerStep = *parsedRate;
        } else if (key == "--world") {
            parsed.worldPath = std::filesystem::path(std::string{value});
        } else if (key == "--layout") {
            parsed.layoutPath = std::filesystem::path(std::string{value});
        } else {
            std::cerr << "Unknown option: " << key << '\n';
            return std::nullopt;
        }
    }

    if (parsed.seed == 0) {
        std::cerr << "Missing required --seed option\n";
        return std::nullopt;
    }

    return parsed;
}

void ensureParentDirectory(const std::filesystem::path& path) {
    if (!path.has_parent_path()) {
        return;
    }
    std::filesystem::create_directories(path.parent_path());
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> arguments(argv + 1, argv + argc);

    auto parsedArgsOpt = parseArguments(arguments);
    if (!parsedArgsOpt.has_value()) {
        printUsage();
        return EXIT_FAILURE;
    }

    if (parsedArgsOpt->help) {
        printUsage();
        return EXIT_SUCCESS;
    }

    ParsedArgs parsedArgs = *parsedArgsOpt;

    NoiseGridConfig config;
    if (parsedArgs.width) {
        config.width = *parsedArgs.width;
    }
    if (parsedArgs.height) {
        config.height = *parsedArgs.height;
    }
    if (parsedArgs.threshold) {
        config.threshold = *parsedArgs.threshold;
    }
    if (parsedArgs.soilDensity) {
        config.soilSpawnDensity = *parsedArgs.soilDensity;
    }
    if (parsedArgs.capacity) {
        config.resourceCapacity = *parsedArgs.capacity;
    }
    if (parsedArgs.ratePerStep) {
        config.resourceRatePerStep = *parsedArgs.ratePerStep;
    }

    try {
        NoiseGridGenerator generator;
        NoiseGenerationResult result = generator.generate(parsedArgs.seed, config);

        ensureParentDirectory(parsedArgs.worldPath);
        ensureParentDirectory(parsedArgs.layoutPath);
        writeNoiseGenerationOutputs(result, parsedArgs.worldPath, parsedArgs.layoutPath);

        const std::size_t soilCount = std::count_if(result.graph.nodes.begin(), result.graph.nodes.end(),
            [](const genesis::world::LocationNode& node) { return node.terrain == "Soil"; });
        const double soilRatio = static_cast<double>(soilCount) /
                                 static_cast<double>(config.width * config.height);

        std::cout << "Generated noise world:\n"
                  << "  Seed: " << parsedArgs.seed << '\n'
                  << "  Grid: " << config.width << 'x' << config.height
                  << " (threshold=" << config.threshold << ")\n"
                  << "  Soil ratio: " << soilRatio << '\n'
                  << "  Nodes: " << result.graph.nodes.size() << '\n'
                  << "  Edges: " << result.graph.edges.size() << '\n'
                  << "  Spawns: " << result.graph.spawns.size() << '\n'
                  << "  World JSON: " << parsedArgs.worldPath << '\n'
                  << "  Layout JSON: " << parsedArgs.layoutPath << '\n';
    } catch (const std::exception& ex) {
        std::cerr << "Generation failed: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
