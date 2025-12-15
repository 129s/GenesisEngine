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

#include "genesis/runtime/Runtime.hpp"
#include "genesis/world/ResourceTypeStrings.hpp"

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
    std::optional<std::filesystem::path> summaryOutPath;
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
           "  --summary-out <file>  Write 1-page Markdown summary for soak\n"
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

    std::vector<std::uint32_t> agentIds;
    agentIds.reserve(initialSnapshot->telemetry.agents.size());
    for (const auto& a : initialSnapshot->telemetry.agents) {
        agentIds.push_back(a.entityId);
    }

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
            plannerTargetCounts[decision.target]++;
            if (decision.target != 0) {
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

            if ((arg == "--root" || arg == "--steps" || arg == "--out" || arg == "--summary-out") && i + 1 >= argc) {
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
            if (arg == "--summary-out") {
                opts.summaryOutPath = std::filesystem::path(argv[++i]);
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
