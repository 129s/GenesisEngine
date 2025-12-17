#include <chrono>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "genesis/agents/Personality.hpp"
#include "genesis/agents/PlannerTargetEncoding.hpp"
#include "genesis/runtime/Runtime.hpp"
#include "genesis/runtime/SchemaVersions.hpp"
#include "genesis/telemetry/SchemaVersions.hpp"
#include "genesis/world/ResourceTypeStrings.hpp"

namespace {

using json = nlohmann::json;

namespace {
[[nodiscard]] std::uint64_t mix_u64(std::uint64_t x) noexcept {
    // splitmix64 finalizer
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

[[nodiscard]] float uniform01_from_u64(std::uint64_t x) noexcept {
    const std::uint64_t v = mix_u64(x) >> 11; // top 53 bits
    return static_cast<float>(static_cast<double>(v) * (1.0 / 9007199254740992.0)); // 2^53
}

[[nodiscard]] genesis::agents::AgentPersonalityBig5 big5FromWorldSeed(std::uint64_t worldSeed, std::uint32_t agentIndex) noexcept {
    const std::uint64_t base = worldSeed ^ (static_cast<std::uint64_t>(agentIndex) * 0xD1B54A32D192ED03ull) ^ 0x9E3779B97F4A7C15ull;
    genesis::agents::AgentPersonalityBig5 p{};
    p.openness = uniform01_from_u64(base ^ 0x11ull);
    p.conscientiousness = uniform01_from_u64(base ^ 0x22ull);
    p.extraversion = uniform01_from_u64(base ^ 0x33ull);
    p.agreeableness = uniform01_from_u64(base ^ 0x44ull);
    p.neuroticism = uniform01_from_u64(base ^ 0x55ull);
    return p;
}
} // namespace

struct CliOptions {
    std::filesystem::path rootPath{"."};
    std::optional<std::filesystem::path> initialWorldFolder;
    std::filesystem::path scriptPath;
    std::uint64_t maxSteps{256};
    std::uint64_t afterSteps{0};
    bool validateSchemas{true};
    bool quiet{false};
    bool exitOnFailure{true};
    std::optional<std::filesystem::path> eventsOutPath;
};

struct SoakOptions {
    std::filesystem::path rootPath{"."};
    std::filesystem::path worldFolder;
    std::optional<std::filesystem::path> worldgenConfigPath;
    std::optional<std::uint64_t> worldgenSeed;
    std::optional<std::filesystem::path> generatedWorldOutFolder;
    std::uint64_t steps{5000};
    std::optional<std::uint64_t> wallSeconds;
    std::uint32_t agentCount{0};
    std::uint64_t progressEvery{0};
    std::uint64_t windowSteps{5000};
    bool validateSchemas{true};
    bool quiet{false};
    std::optional<std::filesystem::path> outPath;
    std::optional<std::filesystem::path> summaryOutPath;
    std::optional<std::filesystem::path> worldlineOutPath;
    std::optional<std::filesystem::path> eventlineOutPath;
    std::optional<std::uint32_t> eventlineAgentId;
    std::optional<std::uint32_t> eventlineAgentIndex;
};

void printUsage(std::ostream& out) {
    out << "genesis-runtime-cli (Windows)\n"
           "\n"
           "Usage:\n"
           "  genesis-runtime-cli run-script <script.json> [options]\n"
           "  genesis-runtime-cli soak [<world-folder>] [options]\n"
           "\n"
           "Options:\n"
           "  --root <path>         Base path for resolving relative paths in script (default: .)\n"
           "  --world <folder>      Optional initial world folder (world.json + map_#.json)\n"
           "  --max-steps <n>       Max steps while waiting for script to finish (default: 256)\n"
           "  --after-steps <n>     Extra steps after script becomes idle (default: 0)\n"
           "  --no-schema-check     Disable schema_version checks (unsafe)\n"
           "  --events-out <file>   Write executed RuntimeEventReport list as JSON\n"
           "  --steps <n>           Steps to simulate for soak (default: 5000)\n"
           "  --wall-seconds <n>    Stop soak after N wall-clock seconds (optional)\n"
            "  --agents <n>          Spawn N agents at start (replaces any existing)\n"
            "  --worldgen-config <file> Generate a world from TOML config before soak\n"
            "  --seed <n>            Seed for world generation (with --worldgen-config)\n"
            "  --generated-world-out <folder> Output folder for generated world DB\n"
            "  --out <file>          Write soak metrics report as JSON\n"
            "  --summary-out <file>  Write 1-page Markdown summary for soak\n"
            "  --worldline-out <file> Write per-window worldline JSONL (macro evidence; not a log)\n"
            "  --eventline-out <file> Write per-event eventline JSONL for one agent (object evolution; sparse facts)\n"
            "  --eventline-agent <id> Agent entityId to observe (with --eventline-out)\n"
            "  --eventline-agent-index <i> Observe i-th agent (sorted by entityId; with --eventline-out)\n"
            "  --window-steps <n>    Window size for worldline (default: 5000)\n"
           "  --progress-every <n>  Print progress every N steps (0 disables)\n"
            "  --quiet               Suppress per-event printing\n"
            "  --no-exit-on-failure  Always return 0 even if some commands fail\n"
            "  --help                Show this help\n";
}

[[nodiscard]] bool validateSnapshotSchemas(bool validateSchemas,
                                           const Genesis::Runtime::SimulationSnapshot& snapshot,
                                           const std::shared_ptr<const Genesis::Runtime::WorldAtlas>& atlas) {
    if (!validateSchemas) {
        return true;
    }

    bool ok = true;
    if (snapshot.telemetry.schema_version != genesis::telemetry::kTickTelemetrySchemaVersion) {
        std::cerr << "Telemetry schema_version mismatch: expected=" << genesis::telemetry::kTickTelemetrySchemaVersion
                  << " actual=" << snapshot.telemetry.schema_version << "\n";
        ok = false;
    }
    if (atlas && atlas->schema_version != Genesis::Runtime::kWorldAtlasSchemaVersion) {
        std::cerr << "WorldAtlas schema_version mismatch: expected=" << Genesis::Runtime::kWorldAtlasSchemaVersion
                  << " actual=" << atlas->schema_version << "\n";
        ok = false;
    }

    if (!ok) {
        std::cerr << "Hint: check whether genesis-runtime-cli.exe and genesis_runtime.dll come from the same build/commit.\n";
    }
    return ok;
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

[[nodiscard]] double giniFromCounts(std::vector<std::uint64_t> counts) {
    if (counts.empty()) {
        return 0.0;
    }
    std::sort(counts.begin(), counts.end());
    std::uint64_t sum = 0;
    for (const auto c : counts) {
        sum += c;
    }
    if (sum == 0) {
        return 0.0;
    }

    // Gini = (2*Σ(i*x_i))/(n*Σx) - (n+1)/n, i=1..n, x sorted ascending
    const double n = static_cast<double>(counts.size());
    double weighted = 0.0;
    for (std::size_t i = 0; i < counts.size(); ++i) {
        weighted += static_cast<double>(i + 1) * static_cast<double>(counts[i]);
    }
    const double g = (2.0 * weighted) / (n * static_cast<double>(sum)) - (n + 1.0) / n;
    return std::clamp(g, 0.0, 1.0);
}

[[nodiscard]] std::string formatPct(double x) {
    if (!std::isfinite(x)) {
        return "n/a";
    }
    const double v = std::clamp(x, 0.0, 1.0) * 100.0;
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << v << "%";
    return oss.str();
}

[[nodiscard]] std::string buildMarkdownSoakSummary(const json& report) {
    std::ostringstream out;

    const auto steps = report.value("steps", 0ULL);
    const auto agentCount = report.value("agentCount", 0U);
    const auto worldFolder = report.value("worldFolder", std::string{});

    out << "# Genesis Soak Summary\n\n";
    out << "- worldFolder: `" << worldFolder << "`\n";
    out << "- steps: `" << steps << "`\n";
    out << "- agentCount: `" << agentCount << "`\n\n";

    if (!report.contains("summary") || !report.at("summary").is_object()) {
        out << "No `summary` found.\n";
        return out.str();
    }

    const auto& summary = report.at("summary");
    const auto& econ = summary.at("resourceEconomy");

    out << "## 资源经济\n\n";
    out << "- totalProduced: `" << econ.value("totalProduced", 0ULL) << "`\n";
    out << "- totalConsumed: `" << econ.value("totalConsumed", 0ULL) << "`\n";
    out << "- workshopProducedTotal: `" << econ.value("workshopProducedTotal", 0ULL) << "`\n";
    out << "- regenProducedTotal: `" << econ.value("regenProducedTotal", 0ULL) << "`\n";
    out << "- netProducedMinusConsumed: `" << report.at("resourceEconomy").value("netProducedMinusConsumed", 0LL) << "`\n";
    out << "- avgUtilization: `" << econ.value("avgUtilization", 0.0) << "`\n";
    out << "- avgOscillation: `" << econ.value("avgOscillation", 0.0) << "`\n";
    out << "- workshopAvgOscillation: `" << econ.value("workshopAvgOscillation", 0.0) << "`\n";
    out << "- totalDecayed: `" << econ.value("totalDecayed", 0ULL) << "`\n";
    out << "- stepsWithAnyDecay: `" << econ.value("stepsWithAnyDecay", 0ULL) << "`\n";
    out << "- stockoutStepsAny: `" << econ.value("stockoutStepsAny", 0ULL) << "`\n";
    out << "- stockoutStepsSources: `" << econ.value("stockoutStepsSources", 0ULL) << "`\n";
    out << "- stockoutStepsWorkshops: `" << econ.value("stockoutStepsWorkshops", 0ULL) << "`\n\n";

    out << "## 资源流量（按类型）\n\n";
    if (summary.contains("resourceFlowByType") && summary.at("resourceFlowByType").is_array()) {
        const auto& flows = summary.at("resourceFlowByType");
        if (flows.empty()) {
            out << "- (none)\n";
        } else {
            for (const auto& row : flows) {
                out << "- " << row.value("type", "?")
                    << " produced=" << row.value("produced", 0ULL)
                    << " consumed=" << row.value("consumed", 0ULL)
                    << " decayed=" << row.value("decayed", 0ULL)
                    << " net=" << row.value("netProducedMinusConsumed", 0LL)
                    << " netAfterDecay=" << row.value("netProducedMinusConsumedMinusDecayed", 0LL)
                    << "\n";
            }
        }
    } else {
        out << "- (missing resourceFlowByType)\n";
    }
    out << "\n";

    out << "## 生产尝试（Workshop）\n\n";
    if (summary.contains("production") && summary.at("production").is_object()) {
        const auto& prod = summary.at("production");
        out << "- attemptsTotal: `" << prod.value("attemptsTotal", 0ULL) << "`\n";
        out << "- succeededTotal: `" << prod.value("succeededTotal", 0ULL) << "`\n";
        out << "- failedTotal: `" << prod.value("failedTotal", 0ULL) << "`\n";

        out << "\n### 失败原因 Top\n\n";
        if (prod.contains("failureReasonsTop") && prod.at("failureReasonsTop").is_array()) {
            const auto& top = prod.at("failureReasonsTop");
            if (top.empty()) {
                out << "- (none)\n";
            } else {
                for (std::size_t i = 0; i < top.size(); ++i) {
                    const auto& r = top.at(i);
                    out << i + 1 << ". " << r.value("reason", "?") << " count=" << r.value("count", 0ULL) << "\n";
                }
            }
        } else {
            out << "- (missing failureReasonsTop)\n";
        }

        out << "\n### 失败点 Top（按交互点）\n\n";
        if (prod.contains("failuresTop") && prod.at("failuresTop").is_array()) {
            const auto& top = prod.at("failuresTop");
            if (top.empty()) {
                out << "- (none)\n";
            } else {
                for (std::size_t i = 0; i < top.size(); ++i) {
                    const auto& b = top.at(i);
                    out << i + 1 << ". "
                        << b.value("type", "?")
                        << (b.value("isWorkshop", true) ? " (Workshop)" : " (Source)")
                        << " map=" << b.value("mapId", 0U)
                        << " id=" << b.value("interactionId", 0U)
                        << " failed=" << b.value("failed", 0ULL)
                        << " name=`" << b.value("name", std::string{}) << "`\n";
                }
            }
        } else {
            out << "- (missing failuresTop)\n";
        }

        out << "\n### 缺输入 Top（可消耗）\n\n";
        if (prod.contains("missingConsumableInputsTop") && prod.at("missingConsumableInputsTop").is_array()) {
            const auto& top = prod.at("missingConsumableInputsTop");
            if (top.empty()) {
                out << "- (none)\n";
            } else {
                for (std::size_t i = 0; i < top.size(); ++i) {
                    const auto& r = top.at(i);
                    out << i + 1 << ". " << r.value("type", "?") << " count=" << r.value("count", 0ULL) << "\n";
                }
            }
        } else {
            out << "- (missing missingConsumableInputsTop)\n";
        }

        out << "\n### 缺输入 Top（非消耗工具/设备）\n\n";
        if (prod.contains("missingNonConsumableInputsTop") && prod.at("missingNonConsumableInputsTop").is_array()) {
            const auto& top = prod.at("missingNonConsumableInputsTop");
            if (top.empty()) {
                out << "- (none)\n";
            } else {
                for (std::size_t i = 0; i < top.size(); ++i) {
                    const auto& r = top.at(i);
                    out << i + 1 << ". " << r.value("type", "?") << " count=" << r.value("count", 0ULL) << "\n";
                }
            }
        } else {
            out << "- (missing missingNonConsumableInputsTop)\n";
        }
    } else {
        out << "- (missing production)\n";
    }
    out << "\n";

    out << "## 资源获取失败（Consume/Take）\n\n";
    if (summary.contains("resourceAttempts") && summary.at("resourceAttempts").is_object()) {
        const auto& ra = summary.at("resourceAttempts");
        out << "- attemptsTotal: `" << ra.value("attemptsTotal", 0ULL) << "`\n";
        out << "- succeededTotal: `" << ra.value("succeededTotal", 0ULL) << "`\n";
        out << "- failedTotal: `" << ra.value("failedTotal", 0ULL) << "`\n";

        out << "\n### 失败原因 Top\n\n";
        if (ra.contains("failureReasonsTop") && ra.at("failureReasonsTop").is_array()) {
            const auto& top = ra.at("failureReasonsTop");
            if (top.empty()) {
                out << "- (none)\n";
            } else {
                for (std::size_t i = 0; i < top.size(); ++i) {
                    const auto& r = top.at(i);
                    out << i + 1 << ". " << r.value("reason", "?") << " count=" << r.value("count", 0ULL) << "\n";
                }
            }
        } else {
            out << "- (missing failureReasonsTop)\n";
        }

        out << "\n### 失败资源类型 Top\n\n";
        if (ra.contains("failureTypesTop") && ra.at("failureTypesTop").is_array()) {
            const auto& top = ra.at("failureTypesTop");
            if (top.empty()) {
                out << "- (none)\n";
            } else {
                for (std::size_t i = 0; i < top.size(); ++i) {
                    const auto& r = top.at(i);
                    out << i + 1 << ". " << r.value("type", "?") << " count=" << r.value("count", 0ULL) << "\n";
                }
            }
        } else {
            out << "- (missing failureTypesTop)\n";
        }

        out << "\n### 失败点 Top（按交互点）\n\n";
        if (ra.contains("failuresTop") && ra.at("failuresTop").is_array()) {
            const auto& top = ra.at("failuresTop");
            if (top.empty()) {
                out << "- (none)\n";
            } else {
                for (std::size_t i = 0; i < top.size(); ++i) {
                    const auto& b = top.at(i);
                    out << i + 1 << ". "
                        << b.value("type", "?")
                        << (b.value("isWorkshop", false) ? " (Workshop)" : " (Source)")
                        << " map=" << b.value("mapId", 0U)
                        << " id=" << b.value("interactionId", 0U)
                        << " failed=" << b.value("failed", 0ULL)
                        << " name=`" << b.value("name", std::string{}) << "`\n";
                }
            }
        } else {
            out << "- (missing failuresTop)\n";
        }
    } else {
        out << "- (missing resourceAttempts)\n";
    }
    out << "\n";

    out << "## 浪费 Top（Decay）\n\n";
    if (summary.contains("decayTop") && summary.at("decayTop").is_array()) {
        const auto& top = summary.at("decayTop");
        if (top.empty()) {
            out << "- (none)\n";
        } else {
            for (std::size_t i = 0; i < top.size(); ++i) {
                const auto& b = top.at(i);
                out << i + 1 << ". "
                    << b.value("type", "?")
                    << (b.value("isWorkshop", false) ? " (Workshop)" : " (Source)")
                    << " map=" << b.value("mapId", 0U)
                    << " id=" << b.value("interactionId", 0U)
                    << " decayed=" << b.value("decayedTotal", 0ULL)
                    << " events=" << b.value("decayEvents", 0ULL)
                    << " name=`" << b.value("name", std::string{}) << "`\n";
            }
        }
    } else {
        out << "- (missing decayTop)\n";
    }
    out << "\n";

    out << "## 动作与行为\n\n";
	    if (summary.contains("actions") && summary.at("actions").is_object() && summary.at("actions").contains("counts")) {
	        const auto& actions = summary.at("actions").at("counts");
	        out << "- ConsumeResource: `" << actions.value("ConsumeResource", 0ULL) << "`\n";
	        out << "- TakeResource: `" << actions.value("TakeResource", 0ULL) << "`\n";
	        out << "- ProduceResource: `" << actions.value("ProduceResource", 0ULL) << "`\n";
	        out << "- MoveToInteraction: `" << actions.value("MoveToInteraction", 0ULL) << "`\n";
	        out << "- SocializeWithAgent: `" << actions.value("SocializeWithAgent", 0ULL) << "`\n";
	    }
	    out << "\n";

    if (summary.contains("switching")) {
        const auto& switching = summary.at("switching");
        out << "- plannerTargetSwitchesPerAgent: `" << switching.value("plannerTargetSwitchesPerAgent", 0.0) << "`\n";
        out << "- produceTargetSwitchesPerAgent: `" << switching.value("produceTargetSwitchesPerAgent", 0.0) << "`\n";
    }
    if (summary.contains("specialization")) {
        const auto& spec = summary.at("specialization");
        out << "- produceTicksGini: `" << spec.value("produceTicksGini", 0.0) << "`\n";
        out << "- takeTicksGini: `" << spec.value("takeTicksGini", 0.0) << "`\n";
    }
    out << "\n";

    out << "## 瓶颈 Top\n\n";
    if (summary.contains("bottlenecksTop") && summary.at("bottlenecksTop").is_array()) {
        const auto& top = summary.at("bottlenecksTop");
        if (top.empty()) {
            out << "- (none)\n";
        } else {
            for (std::size_t i = 0; i < top.size(); ++i) {
                const auto& b = top.at(i);
                const bool isWorkshop = b.value("isWorkshop", false);
                out << i + 1 << ". "
                    << b.value("type", "?")
                    << (isWorkshop ? " (Workshop)" : " (Source)")
                    << " map=" << b.value("mapId", 0U)
                    << " id=" << b.value("interactionId", 0U)
                    << " hits=" << b.value("plannerHits", 0ULL)
                    << (isWorkshop ? (std::string(" fail=") + formatPct(b.value("workshopFailureShare", 0.0)))
                                   : (std::string(" zero=") + formatPct(b.value("zeroShare", 0.0))))
                    << " util=" << formatPct(b.value("avgUtilization", 0.0))
                    << " score=" << b.value("score", 0.0)
                    << " name=`" << b.value("name", std::string{}) << "`\n";
            }
        }
    } else {
        out << "- (missing bottlenecksTop)\n";
    }

    return out.str();
}

struct BottleneckScore {
    std::uint32_t interactionId{0};
    double score{0.0};
};

[[nodiscard]] double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

struct ResourceEconomy {
    std::uint32_t interactionId{0};
    std::uint32_t mapId{0};
    std::string name;
    genesis::world::ResourceType type{genesis::world::ResourceType::Food};
    std::uint32_t capacity{0};
    bool isWorkshop{false};

    std::uint64_t ticks{0};
    std::uint64_t zeroSteps{0};
    double utilizationSum{0.0};
    std::uint64_t utilizationSamples{0};

    std::uint32_t startCurrent{0};
    std::uint32_t endCurrent{0};
    std::uint32_t minCurrent{std::numeric_limits<std::uint32_t>::max()};
    std::uint32_t maxCurrent{0};

    std::uint64_t totalConsumed{0};
    std::uint64_t totalProduced{0};
    std::uint64_t totalDecayed{0};
    std::uint64_t consumptionEvents{0};
    std::uint64_t productionEvents{0};
    std::uint64_t decayEvents{0};
    bool initialized{false};
};

[[nodiscard]] int runSoak(const SoakOptions& opts) {
    if (opts.worldFolder.empty() && !opts.worldgenConfigPath) {
        std::cerr << "Missing world folder (or use --worldgen-config)\n";
        return 2;
    }

    Genesis::Runtime::RuntimeConfig config{};
    if (!opts.worldgenConfigPath) {
        config.initialWorldPath = resolvePathIfRelative(opts.rootPath, opts.worldFolder);
    }
    Genesis::Runtime::Runtime runtime(config);

    std::filesystem::path loadedWorldFolder = config.initialWorldPath ? *config.initialWorldPath : std::filesystem::path{};

    if (opts.worldgenConfigPath) {
        const auto configPath = resolvePathIfRelative(opts.rootPath, *opts.worldgenConfigPath);
        std::optional<std::filesystem::path> outFolder;
        if (opts.generatedWorldOutFolder) {
            outFolder = resolvePathIfRelative(opts.rootPath, *opts.generatedWorldOutFolder);
        }

        const auto gen = runtime.generateWorldFromConfig(configPath, opts.worldgenSeed, outFolder);
        if (!gen.success || !gen.outputPath) {
            std::cerr << "World generation failed: " << (gen.error.empty() ? "unknown error" : gen.error) << "\n";
            for (const auto& line : gen.logs) {
                std::cerr << "  " << line << "\n";
            }
            return 2;
        }

        const auto load = runtime.loadWorldFromFile(*gen.outputPath);
        if (!load.success) {
            std::cerr << "Failed to load generated world: " << (load.error.empty() ? "unknown error" : load.error) << "\n";
            return 2;
        }
        loadedWorldFolder = *gen.outputPath;
    }

    runtime.step(1);
    const auto* initialSnapshot = runtime.latestSnapshot();
    if (!initialSnapshot) {
        std::cerr << "No snapshot available after bootstrap\n";
        return 2;
    }

    std::unordered_map<std::uint32_t, bool> workshopByInteraction;
    workshopByInteraction.reserve(128);
    if (auto db = runtime.worldDatabase()) {
        for (const auto& m : db->maps()) {
            for (const auto& it : db->interactions(m.id)) {
                if (it.kind != genesis::world::InteractionKind::Resource) {
                    continue;
                }
                bool isWorkshop = false;
                if (it.meta) {
                    const auto& meta = *it.meta;
                    isWorkshop = meta.contains("workshop") && meta.at("workshop").is_object();
                }
                workshopByInteraction[it.id] = isWorkshop;
            }
        }
    }

    if (opts.agentCount > 0) {
        std::uint64_t worldSeed = 0;
        if (opts.worldgenSeed) {
            worldSeed = *opts.worldgenSeed;
        } else if (runtime.lastSeed().has_value()) {
            worldSeed = *runtime.lastSeed();
        }

        for (const auto& agent : initialSnapshot->telemetry.agents) {
            runtime.deleteAgent(agent.entityId);
        }
        for (std::uint32_t i = 0; i < opts.agentCount; ++i) {
            Genesis::Simulation::AgentSpawnParams2D params{};
            params.location.mapId = 1;
            params.location.x = 0.0f;
            params.location.y = 0.0f;
            if (worldSeed != 0) {
                params.personality = big5FromWorldSeed(worldSeed, i);
            }
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

    if (!validateSnapshotSchemas(opts.validateSchemas, *initialSnapshot, atlas)) {
        return 3;
    }

    if (opts.windowSteps == 0) {
        std::cerr << "--window-steps must be > 0\n";
        return 2;
    }

    std::unordered_map<std::string, std::uint64_t> actionTypeCounts;
    std::unordered_map<std::uint32_t, std::uint64_t> plannerTargetCounts;
    std::unordered_map<std::uint32_t, ResourceEconomy> resourceByInteraction;

    std::vector<std::uint32_t> agentIds;
    agentIds.reserve(initialSnapshot->telemetry.agents.size());
    for (const auto& a : initialSnapshot->telemetry.agents) {
        agentIds.push_back(a.entityId);
    }
    std::sort(agentIds.begin(), agentIds.end());

    std::unordered_map<std::uint32_t, std::uint64_t> produceTicksByAgent;
    std::unordered_map<std::uint32_t, std::uint64_t> takeTicksByAgent;
    std::unordered_map<std::uint32_t, std::uint32_t> lastProduceTargetByAgent;
    std::unordered_map<std::uint32_t, std::uint32_t> lastPlannerTargetByAgent;
    std::uint64_t produceTargetSwitchesTotal = 0;
    std::uint64_t plannerTargetSwitchesTotal = 0;

    bool sawNeeds = !initialSnapshot->telemetry.needs.empty();
    bool sawPlanner = !initialSnapshot->telemetry.plannerDecisions.empty();
    bool sawActions = !initialSnapshot->telemetry.actions.empty();

    std::uint64_t stockoutStepsAny = 0;
    std::uint64_t stockoutStepsSources = 0;
    std::uint64_t stockoutStepsWorkshops = 0;
    std::uint64_t stepsWithAnyConsumption = 0;
    std::uint64_t stepsWithAnyRegen = 0;
    std::uint64_t stepsWithAnyDecay = 0;

    struct ResourceFlow {
        std::uint64_t produced{0};
        std::uint64_t consumed{0};
        std::uint64_t decayed{0};
    };

    std::unordered_map<std::string, ResourceFlow> flowByType;

    std::uint64_t productionAttemptsTotal = 0;
    std::uint64_t productionSucceededTotal = 0;
    std::uint64_t productionFailedTotal = 0;
    std::unordered_map<std::string, std::uint64_t> productionFailureReasons;
    std::unordered_map<std::uint32_t, std::uint64_t> productionFailuresByWorkshop;
    std::unordered_map<std::uint32_t, std::uint64_t> productionAttemptsByWorkshop;
    std::unordered_map<std::uint32_t, std::uint64_t> productionSucceededByWorkshop;
    std::unordered_map<std::uint32_t, std::uint64_t> productionFailedByWorkshop;

    std::uint64_t resourceAttemptsTotal = 0;
    std::uint64_t resourceSucceededTotal = 0;
    std::uint64_t resourceFailedTotal = 0;
    std::unordered_map<std::string, std::uint64_t> resourceFailureReasons;
    std::unordered_map<std::string, std::uint64_t> resourceFailureByType;
    std::unordered_map<std::uint32_t, std::uint64_t> resourceFailuresByInteraction;

    double utilizationSum = 0.0;
    std::uint64_t utilizationSamples = 0;

	    struct WorldlineWindowAgg {
	        std::uint64_t steps{0};
	        std::uint64_t needSamples{0};
	        std::uint64_t criticalNeedSamples{0};
	        double plannerTravelCostSum{0.0};
	        std::uint64_t plannerTravelCostSamples{0};
	        std::unordered_map<std::string, std::uint64_t> actionTypeCounts;
	        std::unordered_map<std::uint32_t, std::uint64_t> plannerTargetCounts;
	        std::uint64_t socializeEvents{0};
	        std::unordered_map<std::uint32_t, std::uint64_t> socializePartnerCounts;
	        std::uint64_t stockoutInteractionSamplesAnyTotal{0};
	        std::uint64_t stockoutInteractionSamplesAnyZero{0};
	        std::uint64_t stockoutInteractionSamplesSourcesTotal{0};
	        std::uint64_t stockoutInteractionSamplesSourcesZero{0};
	        std::uint64_t stockoutInteractionSamplesWorkshopsTotal{0};
	        std::uint64_t stockoutInteractionSamplesWorkshopsZero{0};
	        std::uint64_t stepsWithAnyConsumption{0};
	        std::uint64_t stepsWithAnyRegen{0};
        std::uint64_t stepsWithAnyDecay{0};
        std::uint64_t productionAttempts{0};
        std::uint64_t productionSucceeded{0};
        std::uint64_t productionFailed{0};
        std::unordered_map<std::string, std::uint64_t> productionFailureReasons;
        std::uint64_t resourceAttempts{0};
        std::uint64_t resourceSucceeded{0};
        std::uint64_t resourceFailed{0};
        std::unordered_map<std::string, std::uint64_t> resourceFailureReasons;

	        void clear() {
	            steps = 0;
	            needSamples = 0;
	            criticalNeedSamples = 0;
	            plannerTravelCostSum = 0.0;
	            plannerTravelCostSamples = 0;
	            actionTypeCounts.clear();
	            plannerTargetCounts.clear();
	            socializeEvents = 0;
	            socializePartnerCounts.clear();
	            stockoutInteractionSamplesAnyTotal = 0;
	            stockoutInteractionSamplesAnyZero = 0;
	            stockoutInteractionSamplesSourcesTotal = 0;
	            stockoutInteractionSamplesSourcesZero = 0;
	            stockoutInteractionSamplesWorkshopsTotal = 0;
	            stockoutInteractionSamplesWorkshopsZero = 0;
	            stepsWithAnyConsumption = 0;
	            stepsWithAnyRegen = 0;
            stepsWithAnyDecay = 0;
            productionAttempts = 0;
            productionSucceeded = 0;
            productionFailed = 0;
            productionFailureReasons.clear();
            resourceAttempts = 0;
            resourceSucceeded = 0;
            resourceFailed = 0;
            resourceFailureReasons.clear();
        }
    };

	    auto buildWorldlineCountsJson = [](const WorldlineWindowAgg& agg) {
	        json actionCountsJson = json::object();
	        for (const auto& [k, v] : agg.actionTypeCounts) {
	            actionCountsJson[k] = v;
	        }

	        json plannerCountsJson = json::object();
	        for (const auto& [k, v] : agg.plannerTargetCounts) {
	            plannerCountsJson[std::to_string(k)] = v;
	        }

	        json socialJson = json::object();
	        socialJson["events"] = agg.socializeEvents;
	        socialJson["uniquePartners"] = agg.socializePartnerCounts.size();
	        socialJson["partnerEntropyBits"] = entropyBitsFromCounts(agg.socializePartnerCounts);

	        json resourceFailureReasonsJson = json::object();
	        for (const auto& [k, v] : agg.resourceFailureReasons) {
	            resourceFailureReasonsJson[k] = v;
	        }

        json productionFailureReasonsJson = json::object();
        for (const auto& [k, v] : agg.productionFailureReasons) {
            productionFailureReasonsJson[k] = v;
        }

	        return json{
	            {"actionTypes", std::move(actionCountsJson)},
	            {"plannerTargets", std::move(plannerCountsJson)},
	            {"social", std::move(socialJson)},
	            {"resourceFailureReasons", std::move(resourceFailureReasonsJson)},
	            {"productionFailureReasons", std::move(productionFailureReasonsJson)},
	        };
	    };

	    auto buildWorldlineMetricsJson = [&](const WorldlineWindowAgg& agg) {
	        const double criticalNeedRate = agg.needSamples > 0 ? static_cast<double>(agg.criticalNeedSamples) / static_cast<double>(agg.needSamples) : 0.0;
	        const double stockoutShareAny = agg.stockoutInteractionSamplesAnyTotal > 0
	            ? static_cast<double>(agg.stockoutInteractionSamplesAnyZero) / static_cast<double>(agg.stockoutInteractionSamplesAnyTotal)
	            : 0.0;
	        const double actionEntropy = entropyBitsFromCounts(agg.actionTypeCounts);
	        const double plannerTargetEntropy = entropyBitsFromCounts(agg.plannerTargetCounts);
	        const double plannerTravelCostMean = agg.plannerTravelCostSamples > 0 ? (agg.plannerTravelCostSum / static_cast<double>(agg.plannerTravelCostSamples)) : 0.0;

	        return json{
	            {"criticalNeedRate", criticalNeedRate},
	            {"stockoutShareAny", stockoutShareAny},
	            {"stockoutShareSources", agg.stockoutInteractionSamplesSourcesTotal > 0
	                ? static_cast<double>(agg.stockoutInteractionSamplesSourcesZero) / static_cast<double>(agg.stockoutInteractionSamplesSourcesTotal)
	                : 0.0},
	            {"stockoutShareWorkshops", agg.stockoutInteractionSamplesWorkshopsTotal > 0
	                ? static_cast<double>(agg.stockoutInteractionSamplesWorkshopsZero) / static_cast<double>(agg.stockoutInteractionSamplesWorkshopsTotal)
	                : 0.0},
	            {"stepsWithAnyConsumption", agg.stepsWithAnyConsumption},
	            {"stepsWithAnyRegen", agg.stepsWithAnyRegen},
	            {"stepsWithAnyDecay", agg.stepsWithAnyDecay},
	            {"actionEntropyBits", actionEntropy},
            {"plannerTargetEntropyBits", plannerTargetEntropy},
            {"plannerTravelCostMean", plannerTravelCostMean},
            {"resourceAttempts", agg.resourceAttempts},
            {"resourceFailed", agg.resourceFailed},
            {"productionAttempts", agg.productionAttempts},
            {"productionFailed", agg.productionFailed},
        };
    };

    std::optional<std::ofstream> worldlineOut;
    if (opts.worldlineOutPath) {
        const auto outputPath = resolvePathIfRelative(opts.rootPath, *opts.worldlineOutPath);
        if (outputPath.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(outputPath.parent_path(), ec);
        }
        worldlineOut.emplace(outputPath, std::ios::out | std::ios::trunc);
        if (!worldlineOut->is_open()) {
            std::cerr << "Failed to write worldline: " << outputPath.string() << "\n";
            return 2;
        }

	        json meta;
	        meta["kind"] = "runtime_worldline_meta";
	        meta["schema_version"] = 2;
	        meta["worldFolder"] = loadedWorldFolder.empty() ? std::string{} : std::filesystem::absolute(loadedWorldFolder).string();
	        if (opts.worldgenConfigPath) {
	            meta["worldgenConfig"] = resolvePathIfRelative(opts.rootPath, *opts.worldgenConfigPath).string();
	        }
        if (opts.worldgenSeed) {
            meta["worldSeed"] = *opts.worldgenSeed;
        } else if (runtime.lastSeed().has_value()) {
            meta["worldSeed"] = *runtime.lastSeed();
        }
        meta["windowSteps"] = opts.windowSteps;
        meta["stepsRequested"] = opts.steps;
        if (opts.wallSeconds) {
            meta["wallSecondsLimit"] = *opts.wallSeconds;
        }
        meta["agentCountRequested"] = opts.agentCount;
        meta["agentCountInitial"] = static_cast<std::uint32_t>(initialSnapshot->telemetry.agents.size());
        meta["telemetrySchemaVersion"] = initialSnapshot->telemetry.schema_version;
        meta["atlas"] = {{"schema_version", atlas->schema_version}, {"world_version", atlas->world_version}};
        meta["worldVersion"] = runtime.worldVersion();
        if (runtime.lastSeed().has_value()) {
            meta["lastSeed"] = *runtime.lastSeed();
        }
        (*worldlineOut) << meta.dump() << "\n";
    }

    // --- Eventline: sparse per-event facts for observing one agent's evolution over time. ---
    std::optional<std::ofstream> eventlineOut;
    std::optional<std::uint32_t> eventlineAgentId;
    struct EventlineState {
        bool initialized{false};
        std::uint32_t entityId{0};
        std::uint32_t lastMapId{0};
        genesis::world::InteractionId lastPlannerTarget{0};
        std::string lastAction;
        std::unordered_map<std::string, float> lastNeedValue;
        std::unordered_map<std::string, bool> lastNeedCritical;

        bool socialInProgress{false};
        std::uint32_t socialPartner{0};
        float socialIntendedRelief{0.0f};
        float socialPrevNeedValue{0.0f};
    };
    EventlineState eventlineState{};

    if (opts.eventlineOutPath) {
        const auto outputPath = resolvePathIfRelative(opts.rootPath, *opts.eventlineOutPath);
        if (outputPath.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(outputPath.parent_path(), ec);
        }
        eventlineOut.emplace(outputPath, std::ios::out | std::ios::trunc);
        if (!eventlineOut->is_open()) {
            std::cerr << "Failed to write eventline: " << outputPath.string() << "\n";
            return 2;
        }

        if (opts.eventlineAgentId) {
            eventlineAgentId = *opts.eventlineAgentId;
        } else if (opts.eventlineAgentIndex) {
            const auto idx = *opts.eventlineAgentIndex;
            if (agentIds.empty() || idx >= agentIds.size()) {
                std::cerr << "--eventline-agent-index out of range (agents=" << agentIds.size() << ")\n";
                return 2;
            }
            eventlineAgentId = agentIds[idx];
        } else {
            if (agentIds.empty()) {
                std::cerr << "No agents available for --eventline-out\n";
                return 2;
            }
            eventlineAgentId = agentIds.front();
        }

        eventlineState.entityId = *eventlineAgentId;
        eventlineState.lastNeedValue.reserve(8);
        eventlineState.lastNeedCritical.reserve(8);

        json meta;
        meta["kind"] = "runtime_eventline_meta";
        meta["schema_version"] = 1;
        meta["worldFolder"] = loadedWorldFolder.empty() ? std::string{} : std::filesystem::absolute(loadedWorldFolder).string();
        if (opts.worldgenConfigPath) {
            meta["worldgenConfig"] = resolvePathIfRelative(opts.rootPath, *opts.worldgenConfigPath).string();
        }
        if (opts.worldgenSeed) {
            meta["worldSeed"] = *opts.worldgenSeed;
        } else if (runtime.lastSeed().has_value()) {
            meta["worldSeed"] = *runtime.lastSeed();
        }
        meta["stepsRequested"] = opts.steps;
        meta["agentCountInitial"] = static_cast<std::uint32_t>(initialSnapshot->telemetry.agents.size());
        meta["telemetrySchemaVersion"] = initialSnapshot->telemetry.schema_version;
        meta["agentEntityId"] = *eventlineAgentId;
        meta["agentIndexSortedByEntityId"] = [&]() -> std::int64_t {
            for (std::size_t i = 0; i < agentIds.size(); ++i) {
                if (agentIds[i] == *eventlineAgentId) return static_cast<std::int64_t>(i);
            }
            return -1;
        }();
        meta["params"] = {
            {"needDeltaAbsMin", 8.0},
            {"socialSuccessDeltaFrac", 0.25},
            {"notes", "eventline is sparse facts for one observed agent; not a full log"},
        };
        (*eventlineOut) << meta.dump() << "\n";
    }

    auto ingestEventlineStep = [&](const genesis::telemetry::TickTelemetry& telemetry) {
        if (!eventlineOut.has_value() || !eventlineAgentId.has_value()) {
            return;
        }

        const std::uint64_t step = telemetry.step;
        const auto entityId = *eventlineAgentId;

        auto emit = [&](const char* type, const json& payload) {
            json e;
            e["kind"] = "runtime_eventline_event";
            e["schema_version"] = 1;
            e["step"] = step;
            e["entityId"] = entityId;
            e["type"] = type;
            e["payload"] = payload;
            (*eventlineOut) << e.dump() << "\n";
        };

        const auto* agentSnap = [&]() -> const genesis::telemetry::AgentSnapshot* {
            for (const auto& a : telemetry.agents) {
                if (a.entityId == entityId) {
                    return &a;
                }
            }
            return nullptr;
        }();

        const auto* plannerSnap = [&]() -> const genesis::telemetry::PlannerSnapshot* {
            for (const auto& p : telemetry.plannerDecisions) {
                if (p.entityId == entityId) {
                    return &p;
                }
            }
            return nullptr;
        }();

        const auto* actionSnap = [&]() -> const genesis::telemetry::ActionSnapshot* {
            for (const auto& a : telemetry.actions) {
                if (a.entityId == entityId) {
                    return &a;
                }
            }
            return nullptr;
        }();

        std::unordered_map<std::string, genesis::telemetry::NeedSnapshot> needsByName;
        needsByName.reserve(8);
        for (const auto& n : telemetry.needs) {
            if (n.entityId != entityId) {
                continue;
            }
            needsByName.emplace(n.needName, n);
        }

        if (!eventlineState.initialized) {
            eventlineState.initialized = true;
            if (agentSnap) {
                eventlineState.lastMapId = agentSnap->mapId;
            }
            if (plannerSnap) {
                eventlineState.lastPlannerTarget = plannerSnap->target;
            }
            if (actionSnap) {
                eventlineState.lastAction = actionSnap->currentAction;
            }
            for (const auto& [name, n] : needsByName) {
                eventlineState.lastNeedValue[name] = n.value;
                eventlineState.lastNeedCritical[name] = n.critical;
            }

            json init;
            init["mapId"] = agentSnap ? agentSnap->mapId : 0U;
            init["pos"] = agentSnap ? json{{"x", agentSnap->position.x}, {"y", agentSnap->position.y}} : json::object();
            init["plannerTarget"] = plannerSnap ? plannerSnap->target : 0U;
            init["action"] = actionSnap ? actionSnap->currentAction : std::string{};
            init["needs"] = json::object();
            for (const auto& [name, n] : needsByName) {
                init["needs"][name] = {{"value", n.value}, {"critical", n.critical}};
            }
            emit("initial_state", init);
        }

        if (agentSnap) {
            if (eventlineState.lastMapId != agentSnap->mapId) {
                emit("map_change", {{"from", eventlineState.lastMapId}, {"to", agentSnap->mapId}});
                eventlineState.lastMapId = agentSnap->mapId;
            }
        }

        if (plannerSnap) {
            if (eventlineState.lastPlannerTarget != plannerSnap->target) {
                emit("planner_target_change",
                     {{"from", eventlineState.lastPlannerTarget},
                      {"to", plannerSnap->target},
                      {"travelCost", plannerSnap->travelCost},
                      {"score", plannerSnap->score},
                      {"toIsAgentTarget", genesis::agents::isAgentPlannerTarget(static_cast<std::uint32_t>(plannerSnap->target))}});
                eventlineState.lastPlannerTarget = plannerSnap->target;
            }
        }

        if (actionSnap) {
            if (eventlineState.lastAction != actionSnap->currentAction) {
                emit("action_change",
                     {{"from", eventlineState.lastAction},
                      {"to", actionSnap->currentAction},
                      {"queueLength", actionSnap->queueLength},
                      {"target", actionSnap->target},
                      {"targetEntityId", actionSnap->targetEntityId},
                      {"resourceTypeId", static_cast<std::uint32_t>(actionSnap->resource)},
                      {"resourceType", genesis::world::resourceTypeName(actionSnap->resource)},
                      {"amount", actionSnap->amount},
                      {"reliefPerUnit", actionSnap->reliefPerUnit},
                      {"speed", actionSnap->speed}});

                if (eventlineState.lastAction == "SocializeWithAgent" && eventlineState.socialInProgress) {
                    float socialNow = eventlineState.socialPrevNeedValue;
                    if (const auto it = needsByName.find("Social"); it != needsByName.end()) {
                        socialNow = it->second.value;
                    }
                    const float delta = socialNow - eventlineState.socialPrevNeedValue;
                    const float intended = std::max(0.0f, eventlineState.socialIntendedRelief);
                    const float successDeltaThreshold = -0.25f * intended;
                    const bool inferredSuccess = (intended > 0.0f) ? (delta <= successDeltaThreshold) : (delta < -0.5f);
                    emit("social_attempt_end",
                         {{"partnerEntityId", eventlineState.socialPartner},
                          {"intendedRelief", intended},
                          {"socialDelta", delta},
                          {"inferredSuccess", inferredSuccess}});
                    eventlineState.socialInProgress = false;
                    eventlineState.socialPartner = 0;
                    eventlineState.socialIntendedRelief = 0.0f;
                }

                if (actionSnap->currentAction == "SocializeWithAgent" && actionSnap->targetEntityId != 0U) {
                    float socialNow = 0.0f;
                    if (const auto it = needsByName.find("Social"); it != needsByName.end()) {
                        socialNow = it->second.value;
                    }
                    eventlineState.socialInProgress = true;
                    eventlineState.socialPartner = actionSnap->targetEntityId;
                    eventlineState.socialIntendedRelief = actionSnap->reliefPerUnit;
                    eventlineState.socialPrevNeedValue = socialNow;
                    emit("social_attempt_start",
                         {{"partnerEntityId", eventlineState.socialPartner}, {"intendedRelief", eventlineState.socialIntendedRelief}, {"socialValue", socialNow}});
                }

                eventlineState.lastAction = actionSnap->currentAction;
            }
        }

        constexpr float kNeedDeltaAbsMin = 8.0f;
        for (const auto& [name, n] : needsByName) {
            const auto prevCritIt = eventlineState.lastNeedCritical.find(name);
            const bool prevCrit = (prevCritIt != eventlineState.lastNeedCritical.end()) ? prevCritIt->second : false;
            if (prevCritIt == eventlineState.lastNeedCritical.end() || prevCrit != n.critical) {
                emit("need_critical_transition", {{"need", name}, {"from", prevCrit}, {"to", n.critical}, {"value", n.value}});
                eventlineState.lastNeedCritical[name] = n.critical;
            }

            const auto prevValIt = eventlineState.lastNeedValue.find(name);
            const float prevVal = (prevValIt != eventlineState.lastNeedValue.end()) ? prevValIt->second : n.value;
            const float dv = n.value - prevVal;
            if (std::abs(dv) >= kNeedDeltaAbsMin) {
                emit("need_delta", {{"need", name}, {"delta", dv}, {"from", prevVal}, {"to", n.value}, {"critical", n.critical}});
            }
            eventlineState.lastNeedValue[name] = n.value;

            if (eventlineState.socialInProgress && name == "Social") {
                eventlineState.socialPrevNeedValue = n.value;
            }
        }

        for (const auto& attempt : telemetry.resourceAttempts) {
            if (attempt.entityId != entityId) {
                continue;
            }
            emit("resource_attempt",
                 {{"action", attempt.action},
                  {"interactionId", attempt.interactionId},
                  {"resourceTypeId", static_cast<std::uint32_t>(attempt.resourceType)},
                  {"resourceType", genesis::world::resourceTypeName(attempt.resourceType)},
                  {"wantedUnits", attempt.wantedUnits},
                  {"obtainedUnits", attempt.obtainedUnits},
                  {"recoveryPlanned", attempt.recoveryPlanned},
                  {"failureReason", attempt.failureReason}});
        }
        for (const auto& attempt : telemetry.workshopAttempts) {
            if (attempt.entityId != entityId) {
                continue;
            }
            emit("workshop_attempt",
                 {{"interactionId", attempt.interactionId},
                  {"outputTypeId", static_cast<std::uint32_t>(attempt.outputType)},
                  {"outputType", genesis::world::resourceTypeName(attempt.outputType)},
                  {"wantedBatches", attempt.wantedBatches},
                  {"wantedUnits", attempt.wantedUnits},
                  {"producedUnits", attempt.producedUnits},
                  {"failureReason", attempt.failureReason}});
        }
    };

    WorldlineWindowAgg windowAgg;
    std::uint64_t worldlineWindowIndex = 0;
    std::uint64_t worldlineWindowStartStep = 0;
    std::uint64_t worldlineStep = 0;

	    auto ingestWorldlineWindow = [&](const genesis::telemetry::TickTelemetry& telemetry) {
	        windowAgg.steps++;
	        windowAgg.needSamples += telemetry.needs.size();
	        for (const auto& need : telemetry.needs) {
            if (need.critical) {
                windowAgg.criticalNeedSamples++;
            }
        }
	        for (const auto& action : telemetry.actions) {
	            windowAgg.actionTypeCounts[action.currentAction]++;
	            if (action.currentAction == "SocializeWithAgent") {
	                windowAgg.socializeEvents++;
	                if (action.targetEntityId != 0U) {
	                    windowAgg.socializePartnerCounts[action.targetEntityId]++;
	                }
	            }
	        }
	        for (const auto& decision : telemetry.plannerDecisions) {
	            if (decision.target == 0U || genesis::agents::isAgentPlannerTarget(static_cast<std::uint32_t>(decision.target))) {
	                continue;
	            }
	            windowAgg.plannerTargetCounts[decision.target]++;
	            if (std::isfinite(decision.travelCost)) {
	                windowAgg.plannerTravelCostSum += static_cast<double>(decision.travelCost);
	                windowAgg.plannerTravelCostSamples++;
	            }
	        }

	        bool anyConsumption = false;
	        bool anyRegen = false;
	        bool anyDecay = false;
	        std::uint64_t stockoutAnyTotal = 0;
	        std::uint64_t stockoutAnyZero = 0;
	        std::uint64_t stockoutSourcesTotal = 0;
	        std::uint64_t stockoutSourcesZero = 0;
	        std::uint64_t stockoutWorkshopsTotal = 0;
	        std::uint64_t stockoutWorkshopsZero = 0;

	        for (const auto& resource : telemetry.resources) {
	            const bool isWorkshop = workshopByInteraction.contains(resource.interactionId) ? workshopByInteraction.at(resource.interactionId) : false;
	            stockoutAnyTotal++;
	            if (isWorkshop) {
	                stockoutWorkshopsTotal++;
	            } else {
	                stockoutSourcesTotal++;
	            }
	            if (resource.current == 0U) {
	                stockoutAnyZero++;
	                if (isWorkshop) {
	                    stockoutWorkshopsZero++;
	                } else {
	                    stockoutSourcesZero++;
	                }
	            }
	            if (resource.consumed > 0U) {
	                anyConsumption = true;
	            }
            if (resource.produced > 0U) {
                anyRegen = true;
            }
            if (resource.decayed > 0U) {
                anyDecay = true;
            }
        }

	        if (anyConsumption) {
	            windowAgg.stepsWithAnyConsumption++;
	        }
	        if (anyRegen) {
            windowAgg.stepsWithAnyRegen++;
        }
	        if (anyDecay) {
	            windowAgg.stepsWithAnyDecay++;
	        }

	        windowAgg.stockoutInteractionSamplesAnyTotal += stockoutAnyTotal;
	        windowAgg.stockoutInteractionSamplesAnyZero += stockoutAnyZero;
	        windowAgg.stockoutInteractionSamplesSourcesTotal += stockoutSourcesTotal;
	        windowAgg.stockoutInteractionSamplesSourcesZero += stockoutSourcesZero;
	        windowAgg.stockoutInteractionSamplesWorkshopsTotal += stockoutWorkshopsTotal;
	        windowAgg.stockoutInteractionSamplesWorkshopsZero += stockoutWorkshopsZero;

	        for (const auto& attempt : telemetry.workshopAttempts) {
	            windowAgg.productionAttempts++;
	            const bool ok = attempt.failureReason.empty() && attempt.producedUnits > 0U;
            if (ok) {
                windowAgg.productionSucceeded++;
            } else {
                windowAgg.productionFailed++;
                if (!attempt.failureReason.empty()) {
                    windowAgg.productionFailureReasons[attempt.failureReason]++;
                }
            }
        }

        for (const auto& attempt : telemetry.resourceAttempts) {
            windowAgg.resourceAttempts++;
            const bool ok = attempt.failureReason.empty() && attempt.obtainedUnits > 0U;
            if (ok) {
                windowAgg.resourceSucceeded++;
            } else {
                windowAgg.resourceFailed++;
                if (!attempt.failureReason.empty()) {
                    windowAgg.resourceFailureReasons[attempt.failureReason]++;
                }
            }
        }
    };

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

            if (action.currentAction == "ProduceResource") {
                produceTicksByAgent[action.entityId]++;
                if (action.target != 0) {
                    const auto prev = lastProduceTargetByAgent.contains(action.entityId) ? lastProduceTargetByAgent[action.entityId] : 0U;
                    if (prev != 0 && prev != action.target) {
                        produceTargetSwitchesTotal++;
                    }
                    lastProduceTargetByAgent[action.entityId] = action.target;
                }
            } else if (action.currentAction == "TakeResource") {
                takeTicksByAgent[action.entityId]++;
            }
        }
	        for (const auto& decision : telemetry.plannerDecisions) {
	            if (decision.target == 0U || genesis::agents::isAgentPlannerTarget(static_cast<std::uint32_t>(decision.target))) {
	                continue;
	            }
	            plannerTargetCounts[decision.target]++;
	            if (decision.target != 0U) {
	                const auto prev = lastPlannerTargetByAgent.contains(decision.entityId) ? lastPlannerTargetByAgent[decision.entityId] : 0U;
	                if (prev != 0 && prev != decision.target) {
	                    plannerTargetSwitchesTotal++;
	                }
	                lastPlannerTargetByAgent[decision.entityId] = decision.target;
	            }
	        }

        bool anyStockout = false;
        bool anyStockoutSource = false;
        bool anyStockoutWorkshop = false;
        bool anyConsumption = false;
        bool anyRegen = false;
        bool anyDecay = false;

        for (const auto& resource : telemetry.resources) {
            {
                const auto typeName = genesis::world::resourceTypeName(resource.type);
                auto& flow = flowByType[typeName];
                flow.produced += resource.produced;
                flow.consumed += resource.consumed;
                flow.decayed += resource.decayed;
            }

            auto& economy = resourceByInteraction[resource.interactionId];
            if (!economy.initialized) {
                economy.initialized = true;
                economy.interactionId = resource.interactionId;
                economy.mapId = resource.mapId;
                economy.name = resource.name;
                economy.type = resource.type;
                economy.capacity = resource.capacity;
                if (auto it = workshopByInteraction.find(resource.interactionId); it != workshopByInteraction.end()) {
                    economy.isWorkshop = it->second;
                }
                economy.startCurrent = resource.current;
            }

            economy.ticks++;
            if (economy.capacity > 0U) {
                economy.utilizationSum += static_cast<double>(resource.current) / static_cast<double>(economy.capacity);
                economy.utilizationSamples++;
            }
            if (resource.current == 0U) {
                economy.zeroSteps++;
            }

            economy.endCurrent = resource.current;
            economy.minCurrent = std::min(economy.minCurrent, resource.current);
            economy.maxCurrent = std::max(economy.maxCurrent, resource.current);
            economy.capacity = resource.capacity;

            if (resource.current == 0U) {
                anyStockout = true;
                if (economy.isWorkshop) {
                    anyStockoutWorkshop = true;
                } else {
                    anyStockoutSource = true;
                }
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
            if (resource.decayed > 0U) {
                economy.totalDecayed += resource.decayed;
                economy.decayEvents++;
                anyDecay = true;
            }

            if (economy.capacity > 0U) {
                utilizationSum += static_cast<double>(economy.endCurrent) / static_cast<double>(economy.capacity);
                utilizationSamples++;
            }
        }

        if (anyStockout) {
            stockoutStepsAny++;
        }
        if (anyStockoutSource) {
            stockoutStepsSources++;
        }
        if (anyStockoutWorkshop) {
            stockoutStepsWorkshops++;
        }
        if (anyConsumption) {
            stepsWithAnyConsumption++;
        }
        if (anyRegen) {
            stepsWithAnyRegen++;
        }
        if (anyDecay) {
            stepsWithAnyDecay++;
        }

        if (!telemetry.workshopAttempts.empty()) {
            for (const auto& attempt : telemetry.workshopAttempts) {
                productionAttemptsTotal++;
                if (attempt.interactionId != 0) {
                    productionAttemptsByWorkshop[attempt.interactionId]++;
                }
                const bool ok = attempt.failureReason.empty() && attempt.producedUnits > 0U;
                if (ok) {
                    productionSucceededTotal++;
                    if (attempt.interactionId != 0) {
                        productionSucceededByWorkshop[attempt.interactionId]++;
                    }
                } else {
                    productionFailedTotal++;
                    if (!attempt.failureReason.empty()) {
                        productionFailureReasons[attempt.failureReason]++;
                        if (attempt.interactionId != 0) {
                            productionFailuresByWorkshop[attempt.interactionId]++;
                            productionFailedByWorkshop[attempt.interactionId]++;
                        }
                    }
                }
            }
        }

        if (!telemetry.resourceAttempts.empty()) {
            for (const auto& attempt : telemetry.resourceAttempts) {
                resourceAttemptsTotal++;
                const bool ok = attempt.failureReason.empty() && attempt.obtainedUnits > 0U;
                if (ok) {
                    resourceSucceededTotal++;
                } else {
                    resourceFailedTotal++;
                    if (!attempt.failureReason.empty()) {
                        resourceFailureReasons[attempt.failureReason]++;
                    }
                    resourceFailureByType[genesis::world::resourceTypeName(attempt.resourceType)]++;
                    if (attempt.interactionId != 0) {
                        resourceFailuresByInteraction[attempt.interactionId]++;
                    }
                }
            }
        }
    };

    ingestTelemetry(initialSnapshot->telemetry);

    const auto startedAt = std::chrono::steady_clock::now();
    const auto worldlineStartedAt = startedAt;
    std::uint64_t stepsExecuted = 0;
    bool endedByWallTime = false;

	    auto flushWorldlineWindow = [&](std::uint64_t windowEndStep) {
	        if (worldlineOut.has_value()) {
	            json window;
	            window["kind"] = "runtime_worldline_window";
	            window["schema_version"] = 2;
	            window["index"] = worldlineWindowIndex;
	            window["stepStart"] = worldlineWindowStartStep;
	            window["stepEnd"] = windowEndStep;
            window["steps"] = (windowEndStep >= worldlineWindowStartStep) ? (windowEndStep - worldlineWindowStartStep + 1) : 0;
            window["elapsedSeconds"] = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - worldlineStartedAt).count();
            window["metrics"] = buildWorldlineMetricsJson(windowAgg);
            window["counts"] = buildWorldlineCountsJson(windowAgg);
            (*worldlineOut) << window.dump() << "\n";
        }

        windowAgg.clear();
        worldlineWindowIndex++;
        worldlineWindowStartStep = windowEndStep + 1;
    };

    auto ingestWorldlineStep = [&](const genesis::telemetry::TickTelemetry& telemetry) {
        ingestWorldlineWindow(telemetry);
        const auto windowLen = (worldlineStep >= worldlineWindowStartStep) ? (worldlineStep - worldlineWindowStartStep + 1) : 0;
        if (windowLen >= opts.windowSteps) {
            flushWorldlineWindow(worldlineStep);
        }
        worldlineStep++;
    };

    if (worldlineOut.has_value()) {
        ingestWorldlineStep(initialSnapshot->telemetry);
    }
    if (eventlineOut.has_value()) {
        ingestEventlineStep(initialSnapshot->telemetry);
    }

    for (std::uint64_t i = 0; i < opts.steps; ++i) {
        if (opts.wallSeconds) {
            const auto elapsedSecs = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startedAt).count();
            if (elapsedSecs >= static_cast<long long>(*opts.wallSeconds)) {
                endedByWallTime = true;
                break;
            }
        }

        runtime.step(1);
        stepsExecuted = i + 1;

        if (opts.progressEvery > 0 && (stepsExecuted % opts.progressEvery) == 0 && !opts.quiet) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - startedAt).count();
            std::cout << "[progress] steps=" << stepsExecuted << " elapsedSeconds=" << std::fixed << std::setprecision(1) << elapsed << "\n";
        }

        const auto* snapshot = runtime.latestSnapshot();
        if (!snapshot) {
            continue;
        }
        ingestTelemetry(snapshot->telemetry);
        if (worldlineOut.has_value()) {
            ingestWorldlineStep(snapshot->telemetry);
        }
        if (eventlineOut.has_value()) {
            ingestEventlineStep(snapshot->telemetry);
        }
    }

    if (worldlineOut.has_value() && windowAgg.steps > 0) {
        flushWorldlineWindow(worldlineStep - 1);
    }

    std::uint64_t totalCapacity = 0;
    std::uint64_t totalFinal = 0;
    std::uint64_t totalConsumed = 0;
    std::uint64_t totalProduced = 0;
    std::uint64_t totalDecayed = 0;
    std::uint64_t workshopProducedTotal = 0;
    std::uint64_t regenProducedTotal = 0;

    json perResource = json::array();
    for (const auto& [interactionId, economy] : resourceByInteraction) {
        (void)interactionId;
        json item;
        item["interactionId"] = economy.interactionId;
        item["mapId"] = economy.mapId;
        item["name"] = economy.name;
        item["type"] = genesis::world::resourceTypeName(economy.type);
        item["typeId"] = static_cast<std::uint32_t>(economy.type);
        item["isWorkshop"] = economy.isWorkshop;
        item["capacity"] = economy.capacity;
        item["start"] = economy.startCurrent;
        item["end"] = economy.endCurrent;
        item["min"] = (economy.minCurrent == std::numeric_limits<std::uint32_t>::max()) ? economy.endCurrent : economy.minCurrent;
        item["max"] = economy.maxCurrent;
        item["consumedTotal"] = economy.totalConsumed;
        item["producedTotal"] = economy.totalProduced;
        item["decayedTotal"] = economy.totalDecayed;
        item["consumeEvents"] = economy.consumptionEvents;
        item["produceEvents"] = economy.productionEvents;
        item["decayEvents"] = economy.decayEvents;
        item["ticks"] = economy.ticks;
        item["zeroSteps"] = economy.zeroSteps;
        item["zeroShare"] = economy.ticks > 0 ? (static_cast<double>(economy.zeroSteps) / static_cast<double>(economy.ticks)) : 0.0;
        item["avgUtilization"] = economy.utilizationSamples > 0 ? (economy.utilizationSum / static_cast<double>(economy.utilizationSamples)) : 0.0;
        item["plannerHits"] = plannerTargetCounts.contains(economy.interactionId) ? plannerTargetCounts.at(economy.interactionId) : 0ULL;
        perResource.push_back(std::move(item));

        totalCapacity += economy.capacity;
        totalFinal += economy.endCurrent;
        totalConsumed += economy.totalConsumed;
        totalProduced += economy.totalProduced;
        totalDecayed += economy.totalDecayed;
        if (economy.isWorkshop) {
            workshopProducedTotal += economy.totalProduced;
        } else {
            regenProducedTotal += economy.totalProduced;
        }
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
    report["worldFolder"] = loadedWorldFolder.empty() ? std::string{} : std::filesystem::absolute(loadedWorldFolder).string();
    if (opts.worldgenConfigPath) {
        report["worldgenConfig"] = resolvePathIfRelative(opts.rootPath, *opts.worldgenConfigPath).string();
    }
    if (opts.worldgenSeed) {
        report["worldSeed"] = *opts.worldgenSeed;
    } else if (runtime.lastSeed().has_value()) {
        report["worldSeed"] = *runtime.lastSeed();
    }
    report["steps"] = stepsExecuted;
    report["stepsRequested"] = opts.steps;
    if (opts.wallSeconds) {
        report["wallSecondsLimit"] = *opts.wallSeconds;
    }
    report["endedByWallTime"] = endedByWallTime;
    report["elapsedSeconds"] = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - startedAt).count();
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
        {"totalDecayed", totalDecayed},
        {"workshopProducedTotal", workshopProducedTotal},
        {"regenProducedTotal", regenProducedTotal},
        {"netProducedMinusConsumed", static_cast<std::int64_t>(totalProduced) - static_cast<std::int64_t>(totalConsumed)},
        // 历史：任意资源点 current==0（包含工坊初始为 0）即计入 `stockoutSteps`。
        {"stockoutSteps", stockoutStepsAny},
        {"stockoutStepsAny", stockoutStepsAny},
        {"stockoutStepsSources", stockoutStepsSources},
        {"stockoutStepsWorkshops", stockoutStepsWorkshops},
        {"stepsWithAnyConsumption", stepsWithAnyConsumption},
        // NOTE: 历史字段名为 `stepsWithAnyRegen`，但此处统计的是 “produced>0”（包含自然 regen 与工坊生产）。
        {"stepsWithAnyProduced", stepsWithAnyRegen},
        {"stepsWithAnyRegen", stepsWithAnyRegen},
        {"stepsWithAnyDecay", stepsWithAnyDecay},
    };

    // --- Summary: Bottlenecks & rollups (human-readable oriented) ---
    std::vector<BottleneckScore> bottlenecks;
    bottlenecks.reserve(resourceByInteraction.size());

    std::uint64_t totalPlannerHits = 0;
    for (const auto& [_, c] : plannerTargetCounts) {
        totalPlannerHits += c;
    }

    for (const auto& [interactionId, economy] : resourceByInteraction) {
        const double zeroShare = economy.ticks > 0 ? (static_cast<double>(economy.zeroSteps) / static_cast<double>(economy.ticks)) : 0.0;
        const double avgUtil = economy.utilizationSamples > 0 ? (economy.utilizationSum / static_cast<double>(economy.utilizationSamples)) : 0.0;
        const auto hitsIt = plannerTargetCounts.find(interactionId);
        const std::uint64_t hits = (hitsIt == plannerTargetCounts.end()) ? 0ULL : hitsIt->second;
        const double hitShare = totalPlannerHits > 0 ? (static_cast<double>(hits) / static_cast<double>(totalPlannerHits)) : 0.0;

        double score = 0.0;

        if (economy.isWorkshop) {
            const std::uint64_t attempts = productionAttemptsByWorkshop.contains(interactionId) ? productionAttemptsByWorkshop.at(interactionId) : 0ULL;
            const std::uint64_t failures = productionFailedByWorkshop.contains(interactionId) ? productionFailedByWorkshop.at(interactionId) : 0ULL;
            const double failShare = attempts > 0 ? (static_cast<double>(failures) / static_cast<double>(attempts)) : 0.0;

            // Workshop score intuition:
            // - A workshop can be "just-in-time" (current stays near 0) while being healthy.
            // - Prefer failure rate over `current==0` for workshop bottleneck detection.
            score = std::log1p(static_cast<double>(hits)) * clamp01(failShare) * (0.25 + clamp01(hitShare) * 2.0);
        } else {
            // Source score intuition:
            // - hot target (hitShare) + frequently empty (zeroShare) => bottleneck candidate.
            // - low avg utilization reinforces (inventory tends to be low).
            score = std::log1p(static_cast<double>(hits)) * (0.65 * clamp01(zeroShare) + 0.35 * (1.0 - clamp01(avgUtil))) * (0.25 + clamp01(hitShare) * 2.0);
        }

        if (score > 0.0) {
            bottlenecks.push_back(BottleneckScore{interactionId, score});
        }
    }

    std::sort(bottlenecks.begin(), bottlenecks.end(), [](const BottleneckScore& a, const BottleneckScore& b) {
        return a.score > b.score;
    });

    std::vector<std::uint64_t> produceTicks;
    std::vector<std::uint64_t> takeTicks;
    produceTicks.reserve(agentIds.size());
    takeTicks.reserve(agentIds.size());
    for (const auto id : agentIds) {
        produceTicks.push_back(produceTicksByAgent.contains(id) ? produceTicksByAgent.at(id) : 0ULL);
        takeTicks.push_back(takeTicksByAgent.contains(id) ? takeTicksByAgent.at(id) : 0ULL);
    }

    double oscillationSum = 0.0;
    std::uint64_t oscillationSamples = 0;
    double workshopOscillationSum = 0.0;
    std::uint64_t workshopOscillationSamples = 0;
    for (const auto& [_, economy] : resourceByInteraction) {
        if (economy.capacity == 0U) {
            continue;
        }
        const auto minV = (economy.minCurrent == std::numeric_limits<std::uint32_t>::max()) ? economy.endCurrent : economy.minCurrent;
        const auto maxV = economy.maxCurrent;
        const double amp = static_cast<double>(maxV - std::min(maxV, minV)) / static_cast<double>(economy.capacity);
        oscillationSum += amp;
        oscillationSamples++;
        if (economy.isWorkshop) {
            workshopOscillationSum += amp;
            workshopOscillationSamples++;
        }
    }

    json topBottlenecks = json::array();
    const std::size_t topN = std::min<std::size_t>(8, bottlenecks.size());
    for (std::size_t i = 0; i < topN; ++i) {
        const auto id = bottlenecks[i].interactionId;
        const auto it = resourceByInteraction.find(id);
        if (it == resourceByInteraction.end()) {
            continue;
        }
        const auto& economy = it->second;
        const double zeroShare = economy.ticks > 0 ? (static_cast<double>(economy.zeroSteps) / static_cast<double>(economy.ticks)) : 0.0;
        const double avgUtil = economy.utilizationSamples > 0 ? (economy.utilizationSum / static_cast<double>(economy.utilizationSamples)) : 0.0;
        const auto hitsIt = plannerTargetCounts.find(id);
        const std::uint64_t hits = (hitsIt == plannerTargetCounts.end()) ? 0ULL : hitsIt->second;

        const std::uint64_t workshopAttempts = productionAttemptsByWorkshop.contains(id) ? productionAttemptsByWorkshop.at(id) : 0ULL;
        const std::uint64_t workshopFailures = productionFailedByWorkshop.contains(id) ? productionFailedByWorkshop.at(id) : 0ULL;
        const double workshopFailureShare = workshopAttempts > 0 ? (static_cast<double>(workshopFailures) / static_cast<double>(workshopAttempts)) : 0.0;

        topBottlenecks.push_back({
            {"interactionId", economy.interactionId},
            {"mapId", economy.mapId},
            {"name", economy.name},
            {"type", genesis::world::resourceTypeName(economy.type)},
            {"isWorkshop", economy.isWorkshop},
            {"capacity", economy.capacity},
            {"end", economy.endCurrent},
            {"consumedTotal", economy.totalConsumed},
            {"producedTotal", economy.totalProduced},
            {"plannerHits", hits},
            {"zeroShare", zeroShare},
            {"avgUtilization", avgUtil},
            {"workshopAttempts", workshopAttempts},
            {"workshopFailures", workshopFailures},
            {"workshopFailureShare", workshopFailureShare},
            {"score", bottlenecks[i].score},
        });
    }

    report["summary"] = {
        {"actions", {{"counts", actionCountsJson}, {"entropy_bits", entropyBitsFromCounts(actionTypeCounts)}}},
        {"plannerTargets", {{"unique", plannerTargetCounts.size()}, {"entropy_bits", entropyBitsFromCounts(plannerTargetCounts)}}},
        {"switching",
         {{"produceTargetSwitchesTotal", produceTargetSwitchesTotal},
          {"produceTargetSwitchesPerAgent", agentIds.empty() ? 0.0 : (static_cast<double>(produceTargetSwitchesTotal) / static_cast<double>(agentIds.size()))},
          {"plannerTargetSwitchesTotal", plannerTargetSwitchesTotal},
          {"plannerTargetSwitchesPerAgent", agentIds.empty() ? 0.0 : (static_cast<double>(plannerTargetSwitchesTotal) / static_cast<double>(agentIds.size()))}}},
        {"specialization",
         {{"produceTicksGini", giniFromCounts(produceTicks)},
          {"takeTicksGini", giniFromCounts(takeTicks)}}},
        {"resourceEconomy",
         {{"totalConsumed", totalConsumed},
          {"totalProduced", totalProduced},
          {"totalDecayed", totalDecayed},
          {"workshopProducedTotal", workshopProducedTotal},
          {"regenProducedTotal", regenProducedTotal},
          {"stockoutStepsAny", stockoutStepsAny},
          {"stockoutStepsSources", stockoutStepsSources},
          {"stockoutStepsWorkshops", stockoutStepsWorkshops},
          {"stepsWithAnyDecay", stepsWithAnyDecay},
          {"avgOscillation", oscillationSamples > 0 ? (oscillationSum / static_cast<double>(oscillationSamples)) : 0.0},
          {"workshopAvgOscillation", workshopOscillationSamples > 0 ? (workshopOscillationSum / static_cast<double>(workshopOscillationSamples)) : 0.0},
          {"avgUtilization", utilizationSamples > 0 ? (utilizationSum / static_cast<double>(utilizationSamples)) : 0.0}}},
        {"bottlenecksTop", topBottlenecks},
    };

    // --- Summary: Resource flow by type (produced/consumed/decayed) ---
    {
        struct FlowRow {
            std::string type;
            std::uint64_t produced{0};
            std::uint64_t consumed{0};
            std::uint64_t decayed{0};
        };
        std::vector<FlowRow> rows;
        rows.reserve(flowByType.size());
        for (const auto& [type, flow] : flowByType) {
            if (flow.produced == 0ULL && flow.consumed == 0ULL && flow.decayed == 0ULL) {
                continue;
            }
            rows.push_back(FlowRow{type, flow.produced, flow.consumed, flow.decayed});
        }
        std::sort(rows.begin(), rows.end(), [](const FlowRow& a, const FlowRow& b) {
            const std::uint64_t wa = a.produced + a.consumed + a.decayed;
            const std::uint64_t wb = b.produced + b.consumed + b.decayed;
            return wa > wb;
        });

        json flowJson = json::array();
        for (const auto& row : rows) {
            flowJson.push_back({
                {"type", row.type},
                {"produced", row.produced},
                {"consumed", row.consumed},
                {"decayed", row.decayed},
                {"netProducedMinusConsumed", static_cast<std::int64_t>(row.produced) - static_cast<std::int64_t>(row.consumed)},
                {"netProducedMinusConsumedMinusDecayed",
                 static_cast<std::int64_t>(row.produced) - static_cast<std::int64_t>(row.consumed) - static_cast<std::int64_t>(row.decayed)},
            });
        }
        report["summary"]["resourceFlowByType"] = std::move(flowJson);
    }

    // --- Summary: Production attempts / failure reasons (explainability) ---
    {
        json reasonsJson = json::object();
        for (const auto& [reason, count] : productionFailureReasons) {
            reasonsJson[reason] = count;
        }

        struct ReasonScore {
            std::string reason;
            std::uint64_t count{0};
        };
        std::vector<ReasonScore> reasons;
        reasons.reserve(productionFailureReasons.size());
        for (const auto& [reason, count] : productionFailureReasons) {
            reasons.push_back(ReasonScore{reason, count});
        }
        std::sort(reasons.begin(), reasons.end(), [](const ReasonScore& a, const ReasonScore& b) {
            return a.count > b.count;
        });

        json reasonsTop = json::array();
        const std::size_t topNReasons = std::min<std::size_t>(8, reasons.size());
        for (std::size_t i = 0; i < topNReasons; ++i) {
            reasonsTop.push_back({{"reason", reasons[i].reason}, {"count", reasons[i].count}});
        }

        struct WorkshopFail {
            std::uint32_t interactionId{0};
            std::uint64_t failed{0};
        };
        std::vector<WorkshopFail> fails;
        fails.reserve(productionFailuresByWorkshop.size());
        for (const auto& [id, failed] : productionFailuresByWorkshop) {
            fails.push_back(WorkshopFail{id, failed});
        }
        std::sort(fails.begin(), fails.end(), [](const WorkshopFail& a, const WorkshopFail& b) {
            return a.failed > b.failed;
        });

        json failuresTop = json::array();
        const std::size_t topN = std::min<std::size_t>(8, fails.size());
        for (std::size_t i = 0; i < topN; ++i) {
            const auto id = fails[i].interactionId;
            const auto it = resourceByInteraction.find(id);
            if (it == resourceByInteraction.end()) {
                failuresTop.push_back({{"interactionId", id}, {"failed", fails[i].failed}});
                continue;
            }
            const auto& economy = it->second;
            failuresTop.push_back({
                {"interactionId", economy.interactionId},
                {"mapId", economy.mapId},
                {"name", economy.name},
                {"type", genesis::world::resourceTypeName(economy.type)},
                {"isWorkshop", economy.isWorkshop},
                {"failed", fails[i].failed},
            });
        }

        auto parseMissingInputType = [](std::string_view reason, std::string_view prefix) -> std::optional<std::string> {
            if (!reason.starts_with(prefix)) {
                return std::nullopt;
            }
            auto rest = reason.substr(prefix.size());
            if (!rest.starts_with(':')) {
                return std::nullopt;
            }
            rest.remove_prefix(1);
            const auto colon = rest.find(':');
            const auto typePart = (colon == std::string_view::npos) ? rest : rest.substr(0, colon);
            if (typePart.empty()) {
                return std::nullopt;
            }
            return std::string(typePart);
        };

        std::unordered_map<std::string, std::uint64_t> missingConsumableInputs;
        std::unordered_map<std::string, std::uint64_t> missingNonConsumableInputs;
        for (const auto& [reason, count] : productionFailureReasons) {
            if (auto type = parseMissingInputType(reason, "MissingConsumableInput")) {
                missingConsumableInputs[*type] += count;
            }
            if (auto type = parseMissingInputType(reason, "MissingNonConsumableInput")) {
                missingNonConsumableInputs[*type] += count;
            }
        }

        auto buildTopList = [](const std::unordered_map<std::string, std::uint64_t>& counts) -> json {
            struct Score {
                std::string key;
                std::uint64_t count{0};
            };
            std::vector<Score> items;
            items.reserve(counts.size());
            for (const auto& [k, v] : counts) {
                items.push_back(Score{k, v});
            }
            std::sort(items.begin(), items.end(), [](const Score& a, const Score& b) { return a.count > b.count; });

            json top = json::array();
            const std::size_t n = std::min<std::size_t>(8, items.size());
            for (std::size_t i = 0; i < n; ++i) {
                top.push_back({{"type", items[i].key}, {"count", items[i].count}});
            }
            return top;
        };

        report["summary"]["production"] = {
            {"attemptsTotal", productionAttemptsTotal},
            {"succeededTotal", productionSucceededTotal},
            {"failedTotal", productionFailedTotal},
            {"failureReasons", std::move(reasonsJson)},
            {"failureReasonsTop", std::move(reasonsTop)},
            {"failuresTop", std::move(failuresTop)},
            {"missingConsumableInputsTop", buildTopList(missingConsumableInputs)},
            {"missingNonConsumableInputsTop", buildTopList(missingNonConsumableInputs)},
        };
    }

    // --- Summary: Resource attempts / failure reasons (Consume/Take) ---
    {
        json reasonsJson = json::object();
        for (const auto& [reason, count] : resourceFailureReasons) {
            reasonsJson[reason] = count;
        }

        struct ReasonScore {
            std::string reason;
            std::uint64_t count{0};
        };
        std::vector<ReasonScore> reasons;
        reasons.reserve(resourceFailureReasons.size());
        for (const auto& [reason, count] : resourceFailureReasons) {
            reasons.push_back(ReasonScore{reason, count});
        }
        std::sort(reasons.begin(), reasons.end(), [](const ReasonScore& a, const ReasonScore& b) {
            return a.count > b.count;
        });

        json reasonsTop = json::array();
        const std::size_t topNReasons = std::min<std::size_t>(8, reasons.size());
        for (std::size_t i = 0; i < topNReasons; ++i) {
            reasonsTop.push_back({{"reason", reasons[i].reason}, {"count", reasons[i].count}});
        }

        struct TypeScore {
            std::string type;
            std::uint64_t count{0};
        };
        std::vector<TypeScore> types;
        types.reserve(resourceFailureByType.size());
        for (const auto& [type, count] : resourceFailureByType) {
            types.push_back(TypeScore{type, count});
        }
        std::sort(types.begin(), types.end(), [](const TypeScore& a, const TypeScore& b) { return a.count > b.count; });

        json typesTop = json::array();
        const std::size_t topNTypes = std::min<std::size_t>(8, types.size());
        for (std::size_t i = 0; i < topNTypes; ++i) {
            typesTop.push_back({{"type", types[i].type}, {"count", types[i].count}});
        }

        struct InteractionFail {
            std::uint32_t interactionId{0};
            std::uint64_t failed{0};
        };
        std::vector<InteractionFail> fails;
        fails.reserve(resourceFailuresByInteraction.size());
        for (const auto& [id, failed] : resourceFailuresByInteraction) {
            fails.push_back(InteractionFail{id, failed});
        }
        std::sort(fails.begin(), fails.end(), [](const InteractionFail& a, const InteractionFail& b) { return a.failed > b.failed; });

        json failuresTop = json::array();
        const std::size_t topN = std::min<std::size_t>(8, fails.size());
        for (std::size_t i = 0; i < topN; ++i) {
            const auto id = fails[i].interactionId;
            const auto it = resourceByInteraction.find(id);
            if (it == resourceByInteraction.end()) {
                failuresTop.push_back({{"interactionId", id}, {"failed", fails[i].failed}});
                continue;
            }
            const auto& economy = it->second;
            failuresTop.push_back({
                {"interactionId", economy.interactionId},
                {"mapId", economy.mapId},
                {"name", economy.name},
                {"type", genesis::world::resourceTypeName(economy.type)},
                {"isWorkshop", economy.isWorkshop},
                {"failed", fails[i].failed},
            });
        }

        report["summary"]["resourceAttempts"] = {
            {"attemptsTotal", resourceAttemptsTotal},
            {"succeededTotal", resourceSucceededTotal},
            {"failedTotal", resourceFailedTotal},
            {"failureReasons", std::move(reasonsJson)},
            {"failureReasonsTop", std::move(reasonsTop)},
            {"failureTypesTop", std::move(typesTop)},
            {"failuresTop", std::move(failuresTop)},
        };
    }

    // --- Summary: Decay / waste top (by totalDecayed) ---
    {
        struct WasteScore {
            std::uint32_t interactionId{0};
            std::uint64_t decayedTotal{0};
        };

        std::vector<WasteScore> waste;
        waste.reserve(resourceByInteraction.size());
        for (const auto& [interactionId, economy] : resourceByInteraction) {
            if (economy.totalDecayed > 0ULL) {
                waste.push_back(WasteScore{interactionId, economy.totalDecayed});
            }
        }
        std::sort(waste.begin(), waste.end(), [](const WasteScore& a, const WasteScore& b) {
            return a.decayedTotal > b.decayedTotal;
        });

        json decayTop = json::array();
        const std::size_t topN = std::min<std::size_t>(8, waste.size());
        for (std::size_t i = 0; i < topN; ++i) {
            const auto id = waste[i].interactionId;
            const auto it = resourceByInteraction.find(id);
            if (it == resourceByInteraction.end()) {
                continue;
            }
            const auto& economy = it->second;
            decayTop.push_back({
                {"interactionId", economy.interactionId},
                {"mapId", economy.mapId},
                {"name", economy.name},
                {"type", genesis::world::resourceTypeName(economy.type)},
                {"isWorkshop", economy.isWorkshop},
                {"decayedTotal", economy.totalDecayed},
                {"decayEvents", economy.decayEvents},
            });
        }

        report["summary"]["decayTop"] = std::move(decayTop);
    }

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

    if (opts.summaryOutPath) {
        const auto outputPath = resolvePathIfRelative(opts.rootPath, *opts.summaryOutPath);
        std::ofstream output(outputPath);
        if (!output.is_open()) {
            std::cerr << "Failed to write summary: " << outputPath.string() << "\n";
            return 2;
        }
        output << buildMarkdownSoakSummary(report);
        if (!opts.quiet) {
            std::cout << "Wrote: " << outputPath.string() << "\n";
        }
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
    bool validatedSchemas = false;

    for (std::uint64_t i = 0; i < opts.maxSteps; ++i) {
        runtime.step(1);
        const auto* snapshot = runtime.latestSnapshot();
        if (!snapshot) {
            continue;
        }

        if (!validatedSchemas) {
            if (!validateSnapshotSchemas(opts.validateSchemas, *snapshot, runtime.worldAtlas())) {
                return 3;
            }
            validatedSchemas = true;
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
            if (arg == "--no-schema-check") {
                opts.validateSchemas = false;
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

        int i = 2;
        if (i < argc) {
            const std::string_view maybeFolder{argv[i]};
            if (!maybeFolder.empty() && maybeFolder.rfind("--", 0) != 0) {
                opts.worldFolder = argv[i];
                i++;
            }
        }

        for (; i < argc; ++i) {
            const std::string_view arg{argv[i]};
            if (arg == "--help" || arg == "-h") {
                return std::nullopt;
            }
            if (arg == "--quiet") {
                opts.quiet = true;
                continue;
            }
            if (arg == "--no-schema-check") {
                opts.validateSchemas = false;
                continue;
            }

            if ((arg == "--root" || arg == "--steps" || arg == "--wall-seconds" || arg == "--out" || arg == "--summary-out" || arg == "--progress-every" ||
                 arg == "--agents" || arg == "--worldline-out" || arg == "--window-steps" || arg == "--worldgen-config" || arg == "--seed" || arg == "--generated-world-out" ||
                 arg == "--eventline-out" || arg == "--eventline-agent" || arg == "--eventline-agent-index") &&
                i + 1 >= argc) {
                return std::nullopt;
            }

            if (arg == "--root") {
                opts.rootPath = argv[++i];
                continue;
            }
            if (arg == "--wall-seconds") {
                std::uint64_t v = 0;
                if (!tryParseU64(argv[++i], v)) {
                    return std::nullopt;
                }
                opts.wallSeconds = v;
                continue;
            }
            if (arg == "--out") {
                opts.outPath = std::filesystem::path(argv[++i]);
                continue;
            }
            if (arg == "--summary-out") {
                opts.summaryOutPath = std::filesystem::path(argv[++i]);
                continue;
            }
            if (arg == "--progress-every") {
                std::uint64_t v = 0;
                if (!tryParseU64(argv[++i], v)) {
                    return std::nullopt;
                }
                opts.progressEvery = v;
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
            if (arg == "--worldline-out") {
                opts.worldlineOutPath = std::filesystem::path(argv[++i]);
                continue;
            }
            if (arg == "--eventline-out") {
                opts.eventlineOutPath = std::filesystem::path(argv[++i]);
                continue;
            }
            if (arg == "--eventline-agent") {
                std::uint64_t v = 0;
                if (!tryParseU64(argv[++i], v) || v > std::numeric_limits<std::uint32_t>::max()) {
                    return std::nullopt;
                }
                opts.eventlineAgentId = static_cast<std::uint32_t>(v);
                continue;
            }
            if (arg == "--eventline-agent-index") {
                std::uint64_t v = 0;
                if (!tryParseU64(argv[++i], v) || v > std::numeric_limits<std::uint32_t>::max()) {
                    return std::nullopt;
                }
                opts.eventlineAgentIndex = static_cast<std::uint32_t>(v);
                continue;
            }
            if (arg == "--window-steps") {
                std::uint64_t v = 0;
                if (!tryParseU64(argv[++i], v)) {
                    return std::nullopt;
                }
                opts.windowSteps = v;
                continue;
            }
            if (arg == "--worldgen-config") {
                opts.worldgenConfigPath = std::filesystem::path(argv[++i]);
                continue;
            }
            if (arg == "--generated-world-out") {
                opts.generatedWorldOutFolder = std::filesystem::path(argv[++i]);
                continue;
            }
            if (arg == "--seed") {
                std::uint64_t v = 0;
                if (!tryParseU64(argv[++i], v)) {
                    return std::nullopt;
                }
                opts.worldgenSeed = v;
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
