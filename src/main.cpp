#include "genesis/core/Engine.hpp"

#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "genesis/telemetry/TelemetryBuffer.hpp"
#include "genesis/world/WorldTypes.hpp"

namespace {

struct RunOptions {
    std::uint64_t steps{600};
    std::string telemetryFile;
    spdlog::level::level_enum logLevel{spdlog::level::info};
};

std::optional<spdlog::level::level_enum> parseLogLevel(std::string_view value) {
    if (value == "trace") return spdlog::level::trace;
    if (value == "debug") return spdlog::level::debug;
    if (value == "info") return spdlog::level::info;
    if (value == "warn" || value == "warning") return spdlog::level::warn;
    if (value == "error") return spdlog::level::err;
    if (value == "critical") return spdlog::level::critical;
    if (value == "off") return spdlog::level::off;
    return std::nullopt;
}

void printUsage() {
    std::cout << "GenesisEngine options:\n"
              << "  --steps=<count>             Number of simulation steps to run (default 600)\n"
              << "  --telemetry-file=<path>     Write telemetry JSON to the given file after the run\n"
              << "  --log-level=<level>         Set log verbosity (trace|debug|info|warn|error|critical|off)\n"
              << "  --help                      Print this help message\n";
}

RunOptions parseOptions(int argc, char** argv) {
    RunOptions options;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            printUsage();
            std::exit(0);
        } else if (arg.rfind("--steps=", 0) == 0) {
            auto value = std::string(arg.substr(8));
            try {
                options.steps = std::stoull(value);
            } catch (const std::exception&) {
                std::cerr << "Invalid value for --steps: " << value << "\n";
                std::exit(1);
            }
        } else if (arg == "--steps") {
            if (i + 1 >= argc) {
                std::cerr << "--steps requires a value\n";
                std::exit(1);
            }
            try {
                options.steps = std::stoull(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "Invalid value for --steps: " << argv[i] << "\n";
                std::exit(1);
            }
        } else if (arg.rfind("--telemetry-file=", 0) == 0) {
            options.telemetryFile = std::string(arg.substr(17));
        } else if (arg == "--telemetry-file") {
            if (i + 1 >= argc) {
                std::cerr << "--telemetry-file requires a path\n";
                std::exit(1);
            }
            options.telemetryFile = argv[++i];
        } else if (arg.rfind("--log-level=", 0) == 0) {
            auto levelStr = std::string_view(arg.substr(12));
            if (auto level = parseLogLevel(levelStr)) {
                options.logLevel = *level;
            } else {
                std::cerr << "Unknown log level: " << levelStr << "\n";
                std::exit(1);
            }
        } else if (arg == "--log-level") {
            if (i + 1 >= argc) {
                std::cerr << "--log-level requires a value\n";
                std::exit(1);
            }
            if (auto level = parseLogLevel(argv[++i])) {
                options.logLevel = *level;
            } else {
                std::cerr << "Unknown log level: " << argv[i] << "\n";
                std::exit(1);
            }
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage();
            std::exit(1);
        }
    }

    return options;
}

nlohmann::json toJson(const genesis::telemetry::TelemetryBuffer& buffer) {
    nlohmann::json root;
    root["ticks"] = nlohmann::json::array();

    for (const auto& tick : buffer.entries()) {
        nlohmann::json tickJson;
        tickJson["step"] = tick.step;

        auto& resources = tickJson["resources"] = nlohmann::json::array();
        for (const auto& res : tick.resources) {
            resources.push_back({
                {"name", res.name},
                {"type", static_cast<int>(res.type)},
                {"location", res.location.value},
                {"current", res.current},
                {"capacity", res.capacity},
            });
        }

        auto& needs = tickJson["needs"] = nlohmann::json::array();
        for (const auto& need : tick.needs) {
            needs.push_back({
                {"entityId", need.entityId},
                {"needName", need.needName},
                {"value", need.value},
                {"critical", need.critical},
            });
        }

        auto& planners = tickJson["planner"] = nlohmann::json::array();
        for (const auto& decision : tick.plannerDecisions) {
            planners.push_back({
                {"entityId", decision.entityId},
                {"target", decision.target.value},
                {"travelCost", decision.travelCost},
                {"score", decision.score},
            });
        }

        auto& actions = tickJson["actions"] = nlohmann::json::array();
        for (const auto& action : tick.actions) {
            actions.push_back({
                {"entityId", action.entityId},
                {"currentAction", action.currentAction},
                {"queueLength", action.queueLength},
                {"target", action.target.value},
                {"speed", action.speed},
                {"resourceType", static_cast<int>(action.resource)},
                {"amount", action.amount},
                {"reliefPerUnit", action.reliefPerUnit},
            });
        }

        auto& agents = tickJson["agents"] = nlohmann::json::array();
        for (const auto& agent : tick.agents) {
            agents.push_back({
                {"entityId", agent.entityId},
                {"location", agent.location.value},
            });
        }

        root["ticks"].push_back(std::move(tickJson));
    }

    return root;
}

void writeTelemetry(const std::string& path, const genesis::telemetry::TelemetryBuffer& buffer) {
    if (path.empty()) {
        return;
    }

    nlohmann::json json = toJson(buffer);
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        spdlog::error("Failed to open telemetry output file {}", path);
        return;
    }

    stream << json.dump(2);
    spdlog::info("Telemetry written to {}", path);
}

} // namespace

int main(int argc, char** argv) {
    const auto options = parseOptions(argc, argv);

    auto logger = spdlog::stdout_color_mt("genesis");
    spdlog::set_default_logger(logger);
    spdlog::set_level(options.logLevel);
    spdlog::info("GenesisEngine bootstrap");

    genesis::core::Engine engine;
    engine.run(options.steps);
    writeTelemetry(options.telemetryFile, engine.telemetry());

    spdlog::info("GenesisEngine shutdown");
    return 0;
}
