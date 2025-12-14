#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
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

void printUsage(std::ostream& out) {
    out << "genesis-runtime-cli (Windows)\n"
           "\n"
           "Usage:\n"
           "  genesis-runtime-cli run-script <script.json> [options]\n"
           "\n"
           "Options:\n"
           "  --root <path>         Base path for resolving relative paths in script (default: .)\n"
           "  --world <folder>      Optional initial world folder (world.json + map_#.json)\n"
           "  --max-steps <n>       Max steps while waiting for script to finish (default: 256)\n"
           "  --after-steps <n>     Extra steps after script becomes idle (default: 0)\n"
           "  --events-out <file>   Write executed RuntimeEventReport list as JSON\n"
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

[[nodiscard]] std::optional<CliOptions> parseArgs(int argc, char** argv) {
    if (argc <= 1) {
        return std::nullopt;
    }

    CliOptions opts;

    const std::string_view cmd{argv[1]};
    if (cmd == "--help" || cmd == "-h") {
        return std::nullopt;
    }
    if (cmd != "run-script") {
        return std::nullopt;
    }
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

    return opts;
}

} // namespace

int main(int argc, char** argv) {
    const auto opts = parseArgs(argc, argv);
    if (!opts) {
        printUsage(std::cerr);
        return 2;
    }

    try {
        return runScript(*opts);
    } catch (const std::exception& ex) {
        std::cerr << "Fatal: " << ex.what() << "\n";
        return 2;
    } catch (...) {
        std::cerr << "Fatal: unknown error\n";
        return 2;
    }
}
