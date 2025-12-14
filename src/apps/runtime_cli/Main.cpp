#include <chrono>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "genesis/runtime/Runtime.hpp"

namespace {

using json = nlohmann::json;

struct CliOptions {
    std::filesystem::path rootPath{"."};
    std::optional<std::filesystem::path> initialWorldFolder;
    std::filesystem::path scriptPath;
    std::uint64_t maxSteps{256};
    std::uint64_t afterSteps{0};
    bool quiet{false};
    bool exitOnFailure{true};
    std::optional<std::filesystem::path> eventsOutPath;
};

struct SoakOptions {
    std::filesystem::path rootPath{"."};
    std::filesystem::path worldFolder;
    std::uint64_t steps{5000};
    std::uint32_t agentCount{0};
    bool quiet{false};
    std::optional<std::filesystem::path> outPath;
};

void printUsage(std::ostream& out) {
    out << "genesis-runtime-cli (Windows)\n"
           "\n"
           "Usage:\n"
           "  genesis-runtime-cli run-script <script.json> [options]\n"
           "  genesis-runtime-cli soak <world-folder> [options]\n"
           "\n"
           "Options:\n"
           "  --root <path>         Base path for resolving relative paths in script (default: .)\n"
           "  --world <folder>      Optional initial world folder (world.json + map_#.json)\n"
           "  --max-steps <n>       Max steps while waiting for script to finish (default: 256)\n"
           "  --after-steps <n>     Extra steps after script becomes idle (default: 0)\n"
           "  --events-out <file>   Write executed RuntimeEventReport list as JSON\n"
           "  --steps <n>           Steps to simulate for soak (default: 5000)\n"
           "  --agents <n>          Spawn N agents at start (replaces any existing)\n"
           "  --out <file>          Write soak metrics report as JSON\n"
           "  --quiet               Suppress per-event printing\n"
           "  --no-exit-on-failure  Always return 0 even if some commands fail\n"
           "  --help                Show this help\n";
}

[[nodiscard]] bool tryParseU64(std::string_view text, std::uint64_t& value) {
    if (text.empty()) {
        return false;
    }
    std::uint64_t v = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
        const auto digit = static_cast<std::uint64_t>(c - '0');
        v = v * 10 + digit;
    }
    value = v;
    return true;
}

[[nodiscard]] std::filesystem::path resolvePathIfRelative(const std::filesystem::path& root, const std::filesystem::path& value) {
    if (value.empty()) {
        return value;
    }
    if (value.is_absolute()) {
        return value;
    }
    return (root / value).lexically_normal();
}

void resolveStringFieldIfRelative(json& obj, const char* key, const std::filesystem::path& root) {
    if (!obj.contains(key) || !obj.at(key).is_string()) {
        return;
    }
    const auto raw = obj.at(key).get<std::string>();
    if (raw.empty()) {
        return;
    }
    const auto resolved = resolvePathIfRelative(root, std::filesystem::path(raw));
    obj[key] = resolved.string();
}

void normalizeScriptPaths(json& script, const std::filesystem::path& root) {
    if (!script.is_object() || !script.contains("commands") || !script.at("commands").is_array()) {
        return;
    }

    for (auto& cmd : script.at("commands")) {
        if (!cmd.is_object()) {
            continue;
        }
        const auto action = cmd.value("action", std::string{});
        if (action == "world.db.load" || action == "world.db.save" || action == "world.db.reload") {
            resolveStringFieldIfRelative(cmd, "folder", root);
            continue;
        }
        if (action == "world.load" || action == "world.save" || action == "world.reload") {
            resolveStringFieldIfRelative(cmd, "path", root);
            continue;
        }
        if (action == "world.db.generate" || action == "world.generate") {
            resolveStringFieldIfRelative(cmd, "configPath", root);
            resolveStringFieldIfRelative(cmd, "config", root);
            resolveStringFieldIfRelative(cmd, "outputFolder", root);
            resolveStringFieldIfRelative(cmd, "outputPath", root);
            resolveStringFieldIfRelative(cmd, "folder", root);
            continue;
        }
    }
}

[[nodiscard]] bool loadJsonFile(const std::filesystem::path& path, json& out, std::string& error) {
    error.clear();
    std::ifstream input(path);
    if (!input.is_open()) {
        error = "Failed to open: " + path.string();
        return false;
    }
    try {
        input >> out;
        return true;
    } catch (const json::parse_error& ex) {
        error = std::string{"Failed to parse JSON: "} + ex.what();
        return false;
    }
}

[[nodiscard]] double entropyBitsFromCounts(const std::unordered_map<std::string, std::uint64_t>& counts) {
    std::uint64_t total = 0;
    for (const auto& [_, c] : counts) {
        total += c;
    }
    if (total == 0) {
        return 0.0;
    }

    double h = 0.0;
    for (const auto& [_, c] : counts) {
        if (c == 0) {
            continue;
        }
        const double p = static_cast<double>(c) / static_cast<double>(total);
        h -= p * std::log2(p);
    }
    return h;
}

[[nodiscard]] double entropyBitsFromCounts(const std::unordered_map<std::uint32_t, std::uint64_t>& counts) {
    std::uint64_t total = 0;
    for (const auto& [_, c] : counts) {
        total += c;
    }
    if (total == 0) {
        return 0.0;
    }

    double h = 0.0;
    for (const auto& [_, c] : counts) {
        if (c == 0) {
            continue;
        }
        const double p = static_cast<double>(c) / static_cast<double>(total);
        h -= p * std::log2(p);
    }
    return h;
}

struct ResourceEconomy {
    std::uint32_t interactionId{0};
    std::uint32_t mapId{0};
    std::string name;
    genesis::world::ResourceType type{genesis::world::ResourceType::Food};
    std::uint32_t capacity{0};

    std::uint32_t startCurrent{0};
    std::uint32_t endCurrent{0};
    std::uint32_t minCurrent{std::numeric_limits<std::uint32_t>::max()};
    std::uint32_t maxCurrent{0};

    std::uint64_t totalConsumed{0};
    std::uint64_t totalProduced{0};
    std::uint64_t consumptionEvents{0};
    std::uint64_t productionEvents{0};
    bool initialized{false};
};

[[nodiscard]] int runSoak(const SoakOptions& opts) {
    if (opts.worldFolder.empty()) {
        std::cerr << "Missing world folder\n";
        return 2;
    }

    Genesis::Runtime::RuntimeConfig config{};
    config.initialWorldPath = resolvePathIfRelative(opts.rootPath, opts.worldFolder);
    Genesis::Runtime::Runtime runtime(config);

    runtime.step(1);
    const auto* initialSnapshot = runtime.latestSnapshot();
    if (!initialSnapshot) {
        std::cerr << "No snapshot available after bootstrap\n";
        return 2;
    }

    if (opts.agentCount > 0) {
        for (const auto& agent : initialSnapshot->telemetry.agents) {
            runtime.deleteAgent(agent.entityId);
        }
        for (std::uint32_t i = 0; i < opts.agentCount; ++i) {
            Genesis::Simulation::AgentSpawnParams2D params{};
            params.location.mapId = 1;
            params.location.x = 0.0f;
            params.location.y = 0.0f;
            (void)runtime.createAgent(params);
        }
        runtime.step(1);
        initialSnapshot = runtime.latestSnapshot();
        if (!initialSnapshot) {
            std::cerr << "No snapshot available after agent spawn\n";
            return 2;
        }
    }

    const auto atlas = runtime.worldAtlas();
    if (!atlas) {
        std::cerr << "WorldAtlas unavailable\n";
        return 2;
    }

    std::unordered_map<std::string, std::uint64_t> actionTypeCounts;
    std::unordered_map<std::uint32_t, std::uint64_t> plannerTargetCounts;
    std::unordered_map<std::uint32_t, ResourceEconomy> resourceByInteraction;

    bool sawNeeds = !initialSnapshot->telemetry.needs.empty();
    bool sawPlanner = !initialSnapshot->telemetry.plannerDecisions.empty();
    bool sawActions = !initialSnapshot->telemetry.actions.empty();

    std::uint64_t stockoutSteps = 0;
    std::uint64_t stepsWithAnyConsumption = 0;
    std::uint64_t stepsWithAnyRegen = 0;

    double utilizationSum = 0.0;
    std::uint64_t utilizationSamples = 0;

    auto ingestTelemetry = [&](const genesis::telemetry::TickTelemetry& telemetry) {
        if (!telemetry.needs.empty()) {
            sawNeeds = true;
        }
        if (!telemetry.plannerDecisions.empty()) {
            sawPlanner = true;
        }
        if (!telemetry.actions.empty()) {
            sawActions = true;
        }

        for (const auto& action : telemetry.actions) {
            actionTypeCounts[action.currentAction]++;
        }
        for (const auto& decision : telemetry.plannerDecisions) {
            plannerTargetCounts[decision.target]++;
        }

        bool anyStockout = false;
        bool anyConsumption = false;
        bool anyRegen = false;

        for (const auto& resource : telemetry.resources) {
            auto& economy = resourceByInteraction[resource.interactionId];
            if (!economy.initialized) {
                economy.initialized = true;
                economy.interactionId = resource.interactionId;
                economy.mapId = resource.mapId;
                economy.name = resource.name;
                economy.type = resource.type;
                economy.capacity = resource.capacity;
                economy.startCurrent = resource.current;
            }

            economy.endCurrent = resource.current;
            economy.minCurrent = std::min(economy.minCurrent, resource.current);
            economy.maxCurrent = std::max(economy.maxCurrent, resource.current);
            economy.capacity = resource.capacity;

            if (resource.current == 0U) {
                anyStockout = true;
            }

            if (resource.consumed > 0U) {
                economy.totalConsumed += resource.consumed;
                economy.consumptionEvents++;
                anyConsumption = true;
            }
            if (resource.produced > 0U) {
                economy.totalProduced += resource.produced;
                economy.productionEvents++;
                anyRegen = true;
            }

            if (economy.capacity > 0U) {
                utilizationSum += static_cast<double>(economy.endCurrent) / static_cast<double>(economy.capacity);
                utilizationSamples++;
            }
        }

        if (anyStockout) {
            stockoutSteps++;
        }
        if (anyConsumption) {
            stepsWithAnyConsumption++;
        }
        if (anyRegen) {
            stepsWithAnyRegen++;
        }
    };

    ingestTelemetry(initialSnapshot->telemetry);

    for (std::uint64_t i = 0; i < opts.steps; ++i) {
        runtime.step(1);
        const auto* snapshot = runtime.latestSnapshot();
        if (!snapshot) {
            continue;
        }
        ingestTelemetry(snapshot->telemetry);
    }

    std::uint64_t totalCapacity = 0;
    std::uint64_t totalFinal = 0;
    std::uint64_t totalConsumed = 0;
    std::uint64_t totalProduced = 0;

    json perResource = json::array();
    for (const auto& [interactionId, economy] : resourceByInteraction) {
        (void)interactionId;
        json item;
        item["interactionId"] = economy.interactionId;
        item["mapId"] = economy.mapId;
        item["name"] = economy.name;
        item["type"] = static_cast<std::uint32_t>(economy.type);
        item["capacity"] = economy.capacity;
        item["start"] = economy.startCurrent;
        item["end"] = economy.endCurrent;
        item["min"] = (economy.minCurrent == std::numeric_limits<std::uint32_t>::max()) ? economy.endCurrent : economy.minCurrent;
        item["max"] = economy.maxCurrent;
        item["consumedTotal"] = economy.totalConsumed;
        item["producedTotal"] = economy.totalProduced;
        item["consumeEvents"] = economy.consumptionEvents;
        item["produceEvents"] = economy.productionEvents;
        perResource.push_back(std::move(item));

        totalCapacity += economy.capacity;
        totalFinal += economy.endCurrent;
        totalConsumed += economy.totalConsumed;
        totalProduced += economy.totalProduced;
    }

    json actionCountsJson = json::object();
    for (const auto& [k, v] : actionTypeCounts) {
        actionCountsJson[k] = v;
    }

    json plannerCountsJson = json::object();
    for (const auto& [k, v] : plannerTargetCounts) {
        plannerCountsJson[std::to_string(k)] = v;
    }

    json report;
    report["kind"] = "runtime_soak_metrics";
    report["worldFolder"] = resolvePathIfRelative(opts.rootPath, opts.worldFolder).string();
    report["steps"] = opts.steps;
    report["agentCount"] = opts.agentCount > 0 ? opts.agentCount : static_cast<std::uint32_t>(initialSnapshot->telemetry.agents.size());
    report["worldVersion"] = runtime.worldVersion();
    report["atlas"] = {{"schema_version", atlas->schema_version}, {"world_version", atlas->world_version}};
    report["telemetrySchemaVersion"] = initialSnapshot->telemetry.schema_version;
    report["liveness"] = {{"sawNeeds", sawNeeds}, {"sawPlanner", sawPlanner}, {"sawActions", sawActions}};
    report["diversity"] = {
        {"actionTypes",
         {{"unique", actionTypeCounts.size()},
          {"entropy_bits", entropyBitsFromCounts(actionTypeCounts)},
          {"counts", actionCountsJson}}},
        {"plannerTargets",
         {{"unique", plannerTargetCounts.size()},
          {"entropy_bits", entropyBitsFromCounts(plannerTargetCounts)},
          {"counts", plannerCountsJson}}},
    };
    report["resourceEconomy"] = {
        {"resources", perResource},
        {"totalCapacity", totalCapacity},
        {"totalFinal", totalFinal},
        {"avgUtilization", utilizationSamples > 0 ? (utilizationSum / static_cast<double>(utilizationSamples)) : 0.0},
        {"totalConsumed", totalConsumed},
        {"totalProduced", totalProduced},
        {"netProducedMinusConsumed", static_cast<std::int64_t>(totalProduced) - static_cast<std::int64_t>(totalConsumed)},
        {"stockoutSteps", stockoutSteps},
        {"stepsWithAnyConsumption", stepsWithAnyConsumption},
        {"stepsWithAnyRegen", stepsWithAnyRegen},
    };

    if (opts.outPath) {
        const auto outputPath = resolvePathIfRelative(opts.rootPath, *opts.outPath);
        std::ofstream output(outputPath);
        if (!output.is_open()) {
            std::cerr << "Failed to write report: " << outputPath.string() << "\n";
            return 2;
        }
        output << report.dump(2) << "\n";
        if (!opts.quiet) {
            std::cout << "Wrote: " << outputPath.string() << "\n";
        }
    } else {
        std::cout << report.dump(2) << "\n";
    }

    return 0;
}

[[nodiscard]] int runScript(const CliOptions& opts) {
    json document;
    std::string error;
    if (!loadJsonFile(opts.scriptPath, document, error)) {
        std::cerr << error << "\n";
        return 2;
    }

    json script;
    if (document.is_array()) {
        script["name"] = opts.scriptPath.filename().string();
        script["commands"] = std::move(document);
    } else {
        script = std::move(document);
    }

    normalizeScriptPaths(script, opts.rootPath);

    Genesis::Runtime::RuntimeConfig config{};
    if (opts.initialWorldFolder) {
        config.initialWorldPath = resolvePathIfRelative(opts.rootPath, *opts.initialWorldFolder);
    }
    Genesis::Runtime::Runtime runtime(config);

    if (!runtime.enqueueCommandSequenceFromJson(script, error)) {
        std::cerr << error << "\n";
        return 2;
    }

    std::vector<Genesis::Runtime::RuntimeEventReport> reports;
    bool anyFailure = false;

    bool sawAnyEvents = false;
    std::uint64_t idleStreak = 0;

    for (std::uint64_t i = 0; i < opts.maxSteps; ++i) {
        runtime.step(1);
        const auto* snapshot = runtime.latestSnapshot();
        if (!snapshot) {
            continue;
        }

        if (!snapshot->events.empty()) {
            sawAnyEvents = true;
            idleStreak = 0;
            reports.insert(reports.end(), snapshot->events.begin(), snapshot->events.end());

            for (const auto& report : snapshot->events) {
                if (!report.success) {
                    anyFailure = true;
                }
                if (!opts.quiet) {
                    std::cout << (report.success ? "[OK] " : "[FAIL] ") << "#" << report.id << " " << report.label;
                    if (!report.message.empty()) {
                        std::cout << " :: " << report.message;
                    }
                    std::cout << "\n";
                }
            }
        } else {
            idleStreak++;
        }

        if (sawAnyEvents && idleStreak >= 1) {
            break;
        }
    }

    if (!sawAnyEvents) {
        std::cerr << "No events executed (script may be empty or max-steps too small)\n";
        return 2;
    }

    if (idleStreak < 1) {
        std::cerr << "Script did not become idle within max-steps=" << opts.maxSteps << "\n";
        return 2;
    }

    for (std::uint64_t i = 0; i < opts.afterSteps; ++i) {
        runtime.step(1);
    }

    if (opts.eventsOutPath) {
        json out = json::array();
        for (const auto& report : reports) {
            json item;
            item["id"] = report.id;
            item["label"] = report.label;
            item["success"] = report.success;
            item["message"] = report.message;
            if (report.payloadJson) {
                item["payloadJson"] = *report.payloadJson;
            }
            out.push_back(std::move(item));
        }

        std::ofstream output(*opts.eventsOutPath);
        if (!output.is_open()) {
            std::cerr << "Failed to write events: " << opts.eventsOutPath->string() << "\n";
            return 2;
        }
        output << out.dump(2) << "\n";
    }

    if (!opts.quiet) {
        std::cout << "worldVersion=" << runtime.worldVersion();
        if (runtime.lastSeed().has_value()) {
            std::cout << " lastSeed=" << *runtime.lastSeed();
        }
        std::cout << "\n";
    }

    if (anyFailure && opts.exitOnFailure) {
        return 1;
    }
    return 0;
}

struct ParsedArgs {
    enum class CommandKind { RunScript, Soak } kind{CommandKind::RunScript};
    CliOptions script;
    SoakOptions soak;
};

[[nodiscard]] std::optional<ParsedArgs> parseArgs(int argc, char** argv) {
    if (argc <= 1) {
        return std::nullopt;
    }

    const std::string_view cmd{argv[1]};
    if (cmd == "--help" || cmd == "-h") {
        return std::nullopt;
    }

    ParsedArgs parsed;

    if (cmd == "run-script") {
        parsed.kind = ParsedArgs::CommandKind::RunScript;
        auto& opts = parsed.script;

        if (argc < 3) {
            return std::nullopt;
        }
        opts.scriptPath = argv[2];

        for (int i = 3; i < argc; ++i) {
            const std::string_view arg{argv[i]};
            if (arg == "--help" || arg == "-h") {
                return std::nullopt;
            }
            if (arg == "--quiet") {
                opts.quiet = true;
                continue;
            }
            if (arg == "--no-exit-on-failure") {
                opts.exitOnFailure = false;
                continue;
            }

            if ((arg == "--root" || arg == "--world" || arg == "--max-steps" || arg == "--after-steps" || arg == "--events-out") && i + 1 >= argc) {
                return std::nullopt;
            }

            if (arg == "--root") {
                opts.rootPath = argv[++i];
                continue;
            }
            if (arg == "--world") {
                opts.initialWorldFolder = std::filesystem::path(argv[++i]);
                continue;
            }
            if (arg == "--events-out") {
                opts.eventsOutPath = std::filesystem::path(argv[++i]);
                continue;
            }
            if (arg == "--max-steps") {
                std::uint64_t v = 0;
                if (!tryParseU64(argv[++i], v)) {
                    return std::nullopt;
                }
                opts.maxSteps = v;
                continue;
            }
            if (arg == "--after-steps") {
                std::uint64_t v = 0;
                if (!tryParseU64(argv[++i], v)) {
                    return std::nullopt;
                }
                opts.afterSteps = v;
                continue;
            }

            return std::nullopt;
        }

        return parsed;
    }

    if (cmd == "soak") {
        parsed.kind = ParsedArgs::CommandKind::Soak;
        auto& opts = parsed.soak;

        if (argc < 3) {
            return std::nullopt;
        }
        opts.worldFolder = argv[2];

        for (int i = 3; i < argc; ++i) {
            const std::string_view arg{argv[i]};
            if (arg == "--help" || arg == "-h") {
                return std::nullopt;
            }
            if (arg == "--quiet") {
                opts.quiet = true;
                continue;
            }

            if ((arg == "--root" || arg == "--steps" || arg == "--out") && i + 1 >= argc) {
                return std::nullopt;
            }

            if (arg == "--root") {
                opts.rootPath = argv[++i];
                continue;
            }
            if (arg == "--out") {
                opts.outPath = std::filesystem::path(argv[++i]);
                continue;
            }
            if (arg == "--steps") {
                std::uint64_t v = 0;
                if (!tryParseU64(argv[++i], v)) {
                    return std::nullopt;
                }
                opts.steps = v;
                continue;
            }
            if (arg == "--agents") {
                std::uint64_t v = 0;
                if (!tryParseU64(argv[++i], v) || v > std::numeric_limits<std::uint32_t>::max()) {
                    return std::nullopt;
                }
                opts.agentCount = static_cast<std::uint32_t>(v);
                continue;
            }

            return std::nullopt;
        }

        return parsed;
    }

    return std::nullopt;
}

} // namespace

int main(int argc, char** argv) {
    const auto opts = parseArgs(argc, argv);
    if (!opts) {
        printUsage(std::cerr);
        return 2;
    }

    try {
        if (opts->kind == ParsedArgs::CommandKind::Soak) {
            return runSoak(opts->soak);
        }
        return runScript(opts->script);
    } catch (const std::exception& ex) {
        std::cerr << "Fatal: " << ex.what() << "\n";
        return 2;
    } catch (...) {
        std::cerr << "Fatal: unknown error\n";
        return 2;
    }
}
