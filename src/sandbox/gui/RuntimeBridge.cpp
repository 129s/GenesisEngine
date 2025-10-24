#include "sandbox/gui/RuntimeBridge.hpp"
#include "genesis/core/Engine.hpp"
#include "genesis/world/WorldDatabaseLoader.hpp"
#include "genesis/world/WorldLoader.hpp"
#include "genesis/world/WorldDatabaseSaver.hpp"
#include <entt/entt.hpp>
#include "genesis/agents/Movement2D.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <climits>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

// Note: avoid including ECS headers here to prevent accidental
// cross-thread access from GUI thread.

namespace Genesis::Sandbox::Gui
{
namespace
{
using json = nlohmann::json;

constexpr float kHorizontalSpacing = 180.0f;
constexpr float kVerticalSpacing = 140.0f;
constexpr float kMinExtent = 100.0f;
constexpr std::size_t kDefaultCommandHistory = 128;
constexpr std::string_view kSourceDirect = "direct";
constexpr std::string_view kSourceScript = "script";
constexpr std::string_view kSourceUI = "ui";

RuntimeBridge::Vector2 computeExtent(std::size_t maxPerLevel, std::size_t levelCount)
{
    const float width = std::max<std::size_t>(1, maxPerLevel) * kHorizontalSpacing;
    const float height = std::max<std::size_t>(1, levelCount) * kVerticalSpacing;
    return RuntimeBridge::Vector2{std::max(width, kMinExtent), std::max(height, kMinExtent)};
}

// 注：buildWorldAtlas(WorldDatabase) 与 loadWorldDatabaseFolder 的定义移至匿名命名空间外部

} // namespace

RuntimeBridge::WorldAtlas RuntimeBridge::buildWorldAtlas(const genesis::world::WorldDatabase& db)
{
    WorldAtlas atlas;

    const auto& maps = db.maps();
    std::size_t levelCount = 1;
    std::size_t maxPerLevel = maps.size();
    atlas.extent = computeExtent(maxPerLevel, levelCount);

    float x = 0.0f;
    for (const auto& m : maps)
    {
        WorldAtlas::Node n{};
        n.id = genesis::world::LocationId{m.id};
        n.parent = genesis::world::InvalidLocation;
        n.kind = genesis::world::LocationKind::Region;
        n.name = m.name;
        n.position = Vector2{x, 0.0f};
        atlas.nodeLookup.emplace(n.id.value, n.position);
        atlas.nodes.push_back(std::move(n));
        x += kHorizontalSpacing;
    }

    for (const auto& e : db.mapEdges())
    {
        WorldAtlas::Edge ae{};
        ae.from = genesis::world::LocationId{e.from};
        ae.to = genesis::world::LocationId{e.to};
        ae.bidirectional = e.bidirectional;
        atlas.edges.push_back(std::move(ae));
    }

    for (const auto& m : maps)
    {
        for (const auto& inter : db.interactions(m.id))
        {
            if (inter.kind == genesis::world::InteractionKind::Resource)
            {
                genesis::world::ResourceSpawn spawn{};
                spawn.name = inter.name;
                spawn.location = genesis::world::LocationId{m.id};
                if (inter.coord.first != 0 || inter.coord.second != 0)
                {
                    spawn.local_coord = std::make_pair(inter.coord.first, inter.coord.second);
                }
                Vector2 position{0.0f, 0.0f};
                if (auto p = atlas.nodePosition(spawn.location)) position = *p;
                atlas.spawns.push_back(WorldAtlas::Spawn{.resource = spawn, .position = position});
            }
        }
    }

    return atlas;
}

bool RuntimeBridge::loadWorldDatabaseFolder(const std::filesystem::path& folder, std::string& errorMessage)
{
    errorMessage.clear();
    auto res = genesis::world::loadWorldDatabaseFromFolder(folder);
    if (!res.success || !res.database)
    {
        errorMessage = res.error.empty() ? std::string("加载 world.json/map_#.json 失败") : res.error;
        return false;
    }

    {
        std::lock_guard lock(controlMutex_);
        worldDb_ = res.database;
    }
    rebuildAtlasOnRuntimeThread();
    return true;
}

RuntimeBridge::RuntimeBridge(genesis::runtime::RuntimeConfig config, std::size_t maxSnapshots)
    : runtime_(std::move(config))
    , maxSnapshots_(std::max<std::size_t>(1, maxSnapshots))
    , atlas_()
    , maxCommandHistory_(kDefaultCommandHistory)
{
    if (auto db = runtime_.worldDatabase()) {
        atlas_ = buildWorldAtlas(*db);
    }
}

RuntimeBridge::~RuntimeBridge()
{
    stop();
    std::vector<std::future<void>> tasks;
    {
        std::lock_guard lock(asyncMutex_);
        tasks.swap(asyncTasks_);
    }
    for (auto& task : tasks)
    {
        if (task.valid())
        {
            task.wait();
        }
    }
}

bool RuntimeBridge::start()
{
    std::lock_guard lock(controlMutex_);
    if (running_)
    {
        return true;
    }

    stopRequested_ = false;
    worker_ = std::thread(&RuntimeBridge::runLoop, this);
    running_ = true;
    return true;
}

void RuntimeBridge::stop()
{
    std::unique_lock lock(controlMutex_);
    if (!running_)
    {
        return;
    }

    stopRequested_ = true;
    controlCv_.notify_all();
    lock.unlock();

    if (worker_.joinable())
    {
        worker_.join();
    }

    lock.lock();
    running_ = false;
    stopRequested_ = false;
    paused_ = false;
    pendingSteps_ = 0;
}

void RuntimeBridge::setPaused(bool paused)
{
    std::lock_guard lock(controlMutex_);
    if (paused_ != paused)
    {
        paused_ = paused;
        controlCv_.notify_all();
    }
}

bool RuntimeBridge::paused() const
{
    std::lock_guard lock(controlMutex_);
    return paused_;
}

void RuntimeBridge::requestStep(std::uint64_t steps)
{
    if (steps == 0)
    {
        return;
    }

    std::lock_guard lock(controlMutex_);
    pendingSteps_ += steps;
    controlCv_.notify_all();
}

void RuntimeBridge::setSpeedMultiplier(double multiplier)
{
    const double clamped = std::clamp(multiplier, 0.1, 16.0);
    std::lock_guard lock(controlMutex_);
    speedMultiplier_ = clamped;
    controlCv_.notify_all();
}

double RuntimeBridge::speedMultiplier() const
{
    std::lock_guard lock(controlMutex_);
    return speedMultiplier_;
}

std::optional<genesis::runtime::Runtime::WorldGenerationResult> RuntimeBridge::generateWorld(const std::filesystem::path& configPath, std::optional<std::uint64_t> seedOverride, std::optional<std::filesystem::path> outputPath)
{
    bool wasRunning = false;
    bool wasPaused = false;
    {
        std::lock_guard lock(controlMutex_);
        wasRunning = running_;
        wasPaused = paused_;
    }

    if (wasRunning)
    {
        stop();
    }

    auto result = runtime_.generateWorldFromConfig(configPath, seedOverride, outputPath);
    {
        std::lock_guard guard(lastGenerationMutex_);
        lastGeneration_ = result;
    }

    if (wasRunning)
    {
        if (start() && wasPaused)
        {
            setPaused(true);
        }
    }

    std::lock_guard guard(lastGenerationMutex_);
    return lastGeneration_;
}

genesis::world::WorldLoadResult RuntimeBridge::loadWorld(const std::filesystem::path& path)
{
    bool wasRunning = false;
    bool wasPaused = false;
    {
        std::lock_guard lock(controlMutex_);
        wasRunning = running_;
        wasPaused = paused_;
    }

    if (wasRunning)
    {
        stop();
    }

    auto result = runtime_.loadWorldFromFile(path);
    if (result.success)
    {
        atlas_ = buildWorldAtlas(runtime_.engine());
        std::lock_guard snapshotLock(snapshotMutex_);
        snapshots_.clear();
    }

    if (wasRunning)
    {
        if (start() && wasPaused)
        {
            setPaused(true);
        }
    }

    return result;
}

genesis::world::WorldSaveResult RuntimeBridge::saveWorld(const std::filesystem::path& path)
{
    bool wasRunning = false;
    bool wasPaused = false;
    {
        std::lock_guard lock(controlMutex_);
        wasRunning = running_;
        wasPaused = paused_;
    }

    if (wasRunning)
    {
        stop();
    }

    auto result = runtime_.saveWorldToFile(path);

    if (wasRunning)
    {
        if (start() && wasPaused)
        {
            setPaused(true);
        }
    }

    return result;
}

std::optional<RuntimeBridge::Snapshot> RuntimeBridge::latestSnapshot() const
{
    std::lock_guard lock(snapshotMutex_);
    if (snapshots_.empty())
    {
        return std::nullopt;
    }
    return snapshots_.back();
}

std::optional<genesis::runtime::Runtime::WorldGenerationResult> RuntimeBridge::lastGeneration() const noexcept
{
    std::lock_guard lock(lastGenerationMutex_);
    return lastGeneration_;
}

std::uint64_t RuntimeBridge::enqueueRuntimeEvent(genesis::runtime::RuntimeEvent event, std::string source)
{
    const auto enqueuedAt = std::chrono::steady_clock::now();
    const auto payload = event.payloadJson;
    const auto kind = event.kind;
    const auto label = event.label;
    const auto id = runtime_.enqueueEvent(std::move(event));
    recordPending(id, kind, label, payload, std::move(source), enqueuedAt);
    return id;
}

std::optional<std::uint64_t> RuntimeBridge::enqueueCommandFromJson(const json& descriptor, std::string source, std::string& errorMessage)
{
    errorMessage.clear();
    if (!descriptor.is_object())
    {
        errorMessage = "Command descriptor must be a JSON object";
        return std::nullopt;
    }

    auto id = enqueueCommandInternal(descriptor, std::move(source), errorMessage);
    return id;
}

bool RuntimeBridge::enqueueCommandSequence(const json& script, std::string source, std::string& errorMessage)
{
    errorMessage.clear();
    if (!script.is_object())
    {
        errorMessage = "Sequence script must be a JSON object";
        return false;
    }

    if (!script.contains("commands") || !script.at("commands").is_array())
    {
        errorMessage = "Script is missing commands array";
        return false;
    }

    auto sequence = std::make_shared<CommandSequence>();
    sequence->name = script.value("name", std::string{"sequence"});
    sequence->source = std::move(source);
    sequence->commands.reserve(script.at("commands").size());
    for (const auto& item : script.at("commands"))
    {
        if (!item.is_object())
        {
            errorMessage = "Commands array entries must be JSON objects";
            return false;
        }
        ScriptCommand command;
        json descriptor = item;
        if (descriptor.contains("waitForSuccess"))
        {
            if (!descriptor.at("waitForSuccess").is_boolean())
            {
                errorMessage = "'waitForSuccess' must be a boolean";
                return false;
            }
            command.waitForSuccess = descriptor.at("waitForSuccess").get<bool>();
            descriptor.erase("waitForSuccess");
        }
        command.descriptor = std::move(descriptor);
        sequence->commands.push_back(std::move(command));
    }

    {
        std::lock_guard seqLock(sequenceMutex_);
        sequences_.push_back(sequence);
    }

    if (!scheduleSequence(sequence, errorMessage))
    {
        return false;
    }

    return true;
}

bool RuntimeBridge::enqueueCommandScript(const std::filesystem::path& scriptPath, std::string source, std::string& errorMessage)
{
    errorMessage.clear();

    std::ifstream input(scriptPath);
    if (!input.is_open())
    {
        errorMessage = "Failed to open script file: " + scriptPath.string();
        return false;
    }

    try
    {
        json document;
        input >> document;
        if (document.is_array())
        {
            json wrapper;
            wrapper["name"] = scriptPath.filename().string();
            wrapper["commands"] = document;
            return enqueueCommandSequence(wrapper, std::move(source), errorMessage);
        }
        return enqueueCommandSequence(document, std::move(source), errorMessage);
    }
    catch (const json::parse_error& ex)
    {
        errorMessage = std::string{"Failed to parse script: "} + ex.what();
        return false;
    }
}

std::vector<RuntimeBridge::CommandProgress> RuntimeBridge::commandStatusSnapshot() const
{
    std::vector<CommandProgress> snapshot;
    std::lock_guard lock(commandMutex_);
    snapshot.reserve(pendingCommands_.size() + commandHistory_.size());
    for (const auto& [_, command] : pendingCommands_)
    {
        snapshot.push_back(command);
    }
    for (const auto& command : commandHistory_)
    {
        snapshot.push_back(command);
    }
    std::sort(snapshot.begin(), snapshot.end(), [](const CommandProgress& lhs, const CommandProgress& rhs) {
        return lhs.enqueuedAt < rhs.enqueuedAt;
    });
    return snapshot;
}

std::uint64_t RuntimeBridge::recordPending(std::uint64_t id, genesis::runtime::RuntimeEventKind kind, std::string label, std::optional<std::string> payload, std::string source, std::chrono::steady_clock::time_point enqueuedAt)
{
    CommandProgress progress;
    progress.id = id;
    progress.kind = kind;
    progress.label = std::move(label);
    progress.payloadJson = std::move(payload);
    progress.source = std::move(source);
    progress.state = CommandState::Pending;
    progress.enqueuedAt = enqueuedAt;

    std::lock_guard lock(commandMutex_);
    pendingCommands_[id] = std::move(progress);
    return id;
}

void RuntimeBridge::completeCommand(std::uint64_t id, bool success, std::string message, std::optional<std::string> payloadOverride)
{
    CommandProgress completed;
    {
        std::lock_guard lock(commandMutex_);
        auto it = pendingCommands_.find(id);
        if (it == pendingCommands_.end())
        {
            return;
        }
        CommandProgress &stored = it->second;
        stored.state = success ? CommandState::Succeeded : CommandState::Failed;
        stored.executedAt = std::chrono::steady_clock::now();
        if (payloadOverride)
        {
            stored.payloadJson = std::move(payloadOverride);
        }
        if (!message.empty())
        {
            stored.message = std::move(message);
        }
        completed = stored;
        commandHistory_.push_back(stored);
        pendingCommands_.erase(it);
        if (commandHistory_.size() > maxCommandHistory_)
        {
            commandHistory_.pop_front();
        }
    }

    genesis::runtime::RuntimeEventReport report{};
    report.id = completed.id;
    report.kind = completed.kind;
    report.label = completed.label;
    if (completed.payloadJson)
    {
        report.payloadJson = completed.payloadJson;
    }
    report.enqueuedAt = completed.enqueuedAt;
    report.executedAt = completed.executedAt.value_or(std::chrono::steady_clock::now());
    report.success = success;
    report.message = completed.message;
    advanceSequencesFor({report});
}

void RuntimeBridge::purgeFinishedTasks()
{
    std::lock_guard lock(asyncMutex_);
    auto it = asyncTasks_.begin();
    while (it != asyncTasks_.end())
    {
        if (!it->valid() || it->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            if (it->valid())
            {
                it->wait();
            }
            it = asyncTasks_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void RuntimeBridge::reconcileCommands(const std::vector<genesis::runtime::RuntimeEventReport>& reports)
{
    if (reports.empty())
    {
        return;
    }

    std::lock_guard lock(commandMutex_);
    for (const auto& report : reports)
    {
        auto it = pendingCommands_.find(report.id);
        if (it == pendingCommands_.end())
        {
            continue;
        }

        CommandProgress progress = it->second;
        progress.state = report.success ? CommandState::Succeeded : CommandState::Failed;
        progress.executedAt = report.executedAt;
        if (report.payloadJson.has_value())
        {
            progress.payloadJson = report.payloadJson;
        }
        if (!report.message.empty())
        {
            progress.message = report.message;
        }
        else if (!report.success)
        {
            progress.message = "Command failed (no details provided)";
        }

        pendingCommands_.erase(it);
        commandHistory_.push_back(std::move(progress));
        if (commandHistory_.size() > maxCommandHistory_)
        {
            commandHistory_.pop_front();
        }
    }
}

void RuntimeBridge::advanceSequencesFor(const std::vector<genesis::runtime::RuntimeEventReport>& reports)
{
    if (reports.empty())
    {
        return;
    }

    std::vector<CommandSequencePtr> toSchedule;

    {
        std::lock_guard seqLock(sequenceMutex_);
        for (const auto& report : reports)
        {
            for (auto& sequence : sequences_)
            {
                if (!sequence || sequence->aborted)
                {
                    continue;
                }

                if (sequence->waitingOn && sequence->waitingOn.value() == report.id)
                {
                    if (report.success)
                    {
                        sequence->waitingOn.reset();
                        if (std::find(toSchedule.begin(), toSchedule.end(), sequence) == toSchedule.end())
                        {
                            toSchedule.push_back(sequence);
                        }
                    }
                    else
                    {
                        sequence->aborted = true;
                        sequence->errorMessage = report.message.empty() ? "Dependent command failed" : report.message;
                    }
                }
            }
        }
    }

    for (auto& sequence : toSchedule)
    {
        if (!sequence || sequence->aborted)
        {
            continue;
        }
        std::string error;
        if (!scheduleSequence(sequence, error) && !error.empty())
        {
            std::lock_guard seqLock(sequenceMutex_);
            sequence->aborted = true;
            sequence->errorMessage = error;
        }
    }

    {
        std::lock_guard seqLock(sequenceMutex_);
        for (auto it = sequences_.begin(); it != sequences_.end();)
        {
            const auto& seq = *it;
            if (!seq)
            {
                it = sequences_.erase(it);
                continue;
            }

            const bool finished = !seq->waitingOn && seq->nextIndex >= seq->commands.size();
            if (finished || seq->aborted)
            {
                it = sequences_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}

bool RuntimeBridge::scheduleSequence(const CommandSequencePtr& sequence, std::string& errorMessage)
{
    errorMessage.clear();
    if (!sequence)
    {
        return true;
    }

    while (true)
    {
        json descriptor;
        bool waitForSuccess = false;
        {
            std::lock_guard seqLock(sequenceMutex_);
            if (sequence->aborted)
            {
                errorMessage = sequence->errorMessage;
                return false;
            }
            if (sequence->waitingOn)
            {
                return true;
            }
            if (sequence->nextIndex >= sequence->commands.size())
            {
                return true;
            }

            const auto idx = sequence->nextIndex;
            descriptor = sequence->commands[idx].descriptor;
            waitForSuccess = sequence->commands[idx].waitForSuccess;
            sequence->nextIndex++;
        }

        std::string localError;
        auto id = enqueueCommandInternal(descriptor, sequence->source, localError);
        if (!id.has_value())
        {
            errorMessage = localError;
            return false;
        }

        if (waitForSuccess)
        {
            std::lock_guard seqLock(sequenceMutex_);
            sequence->waitingOn = id;
            return true;
        }
    }
}

std::optional<std::uint64_t> RuntimeBridge::enqueueCommandInternal(const json& command, std::string source, std::string& errorMessage)
{
    errorMessage.clear();
    if (!command.contains("action") || !command.at("action").is_string())
    {
        errorMessage = "Command is missing string field 'action'";
        return std::nullopt;
    }

    const auto action = command.at("action").get<std::string>();
    if (action == "world.generate")
    {
        return enqueueWorldGenerationCommand(command, std::move(source), errorMessage);
    }
    if (action == "world.load" || action == "world.reload")
    {
        return enqueueWorldReloadCommand(command, std::move(source), errorMessage);
    }
    if (action == "world.save")
    {
        return enqueueWorldSaveCommand(command, std::move(source), errorMessage);
    }
    if (action == "agent.create2d")
    {
        errorMessage.clear();
        const auto payload = command.dump();
        const auto label = command.value("label", std::string{"agent.create2d"});
        const auto enqueuedAt = std::chrono::steady_clock::now();
        const auto id = recordPending(nextManualCommandId_.fetch_add(1), genesis::runtime::RuntimeEventKind::Command, label, payload, std::move(source), enqueuedAt);
        purgeFinishedTasks();
        auto task = std::async(std::launch::async, [this, id, command]() {
            bool success = false;
            std::string message;
            try {
                genesis::runtime::RuntimeEvent ev;
                ev.kind = genesis::runtime::RuntimeEventKind::Command;
                ev.label = "agent.create2d";
                ev.payloadJson = command.dump();
                ev.runtimeHandler = [command, &success, &message](genesis::runtime::Runtime& runtime) {
                    auto& reg = runtime.engine().registry();
                    auto e = reg.create();
                    genesis::agents::components::AgentLocation2D loc{};
                    loc.mapId = command.value("mapId", 1U);
                    loc.x = command.value("x", 0.0f);
                    loc.y = command.value("y", 0.0f);
                    reg.emplace<genesis::agents::components::AgentLocation2D>(e, loc);
                    if (command.contains("move") && command["move"].is_object()) {
                        const auto& mv = command["move"];
                        genesis::agents::components::MovementIntent2D intent{};
                        intent.targetMapId = mv.value("mapId", loc.mapId);
                        intent.targetX = mv.value("x", loc.x);
                        intent.targetY = mv.value("y", loc.y);
                        intent.speed = mv.value("speed", 1.0f);
                        reg.emplace<genesis::agents::components::MovementIntent2D>(e, intent);
                    }
                    success = true;
                    message = std::string("created entity ") + std::to_string(static_cast<std::uint32_t>(entt::to_integral(e)));
                };
                auto rid = runtime_.enqueueEvent(std::move(ev));
                (void)rid;
            } catch (const std::exception& ex) {
                message = ex.what();
            } catch (...) {
                message = "agent.create2d unknown error";
            }
            completeCommand(id, success, std::move(message));
        });
        {
            std::lock_guard lock(asyncMutex_);
            asyncTasks_.push_back(std::move(task));
        }
        return id;
    }
    if (action == "agent.move2d")
    {
        errorMessage.clear();
        if (!command.contains("entityId") || !command.at("entityId").is_number_unsigned()) {
            errorMessage = "'agent.move2d' requires entityId";
            return std::nullopt;
        }
        const auto payload = command.dump();
        const auto label = command.value("label", std::string{"agent.move2d"});
        const auto enqueuedAt = std::chrono::steady_clock::now();
        const auto id = recordPending(nextManualCommandId_.fetch_add(1), genesis::runtime::RuntimeEventKind::Command, label, payload, std::move(source), enqueuedAt);
        purgeFinishedTasks();
        auto task = std::async(std::launch::async, [this, id, command]() {
            bool success = false;
            std::string message;
            try {
                genesis::runtime::RuntimeEvent ev;
                ev.kind = genesis::runtime::RuntimeEventKind::Command;
                ev.label = "agent.move2d";
                ev.payloadJson = command.dump();
                ev.runtimeHandler = [command, &success, &message](genesis::runtime::Runtime& runtime) {
                    auto& reg = runtime.engine().registry();
                    const auto entId = static_cast<entt::entity>(command.at("entityId").get<std::uint32_t>());
                    if (!reg.valid(entId)) { throw std::runtime_error("entity not found"); }
                    genesis::agents::components::MovementIntent2D intent{};
                    intent.targetMapId = command.value("mapId", reg.get<genesis::agents::components::AgentLocation2D>(entId).mapId);
                    intent.targetX = command.value("x", 0.0f);
                    intent.targetY = command.value("y", 0.0f);
                    intent.speed = command.value("speed", 1.0f);
                    if (auto* existing = reg.try_get<genesis::agents::components::MovementIntent2D>(entId)) {
                        *existing = intent;
                    } else {
                        reg.emplace<genesis::agents::components::MovementIntent2D>(entId, intent);
                    }
                    success = true;
                    message = "move intent set";
                };
                auto rid = runtime_.enqueueEvent(std::move(ev));
                (void)rid;
            } catch (const std::exception& ex) {
                message = ex.what();
            } catch (...) {
                message = "agent.move2d unknown error";
            }
            completeCommand(id, success, std::move(message));
        });
        {
            std::lock_guard lock(asyncMutex_);
            asyncTasks_.push_back(std::move(task));
        }
        return id;
    }
    if (action == "agent.delete2d")
    {
        errorMessage.clear();
        if (!command.contains("entityId") || !command.at("entityId").is_number_unsigned()) {
            errorMessage = "'agent.delete2d' requires entityId";
            return std::nullopt;
        }
        const auto payload = command.dump();
        const auto label = command.value("label", std::string{"agent.delete2d"});
        const auto enqueuedAt = std::chrono::steady_clock::now();
        const auto id = recordPending(nextManualCommandId_.fetch_add(1), genesis::runtime::RuntimeEventKind::Command, label, payload, std::move(source), enqueuedAt);
        purgeFinishedTasks();
        auto task = std::async(std::launch::async, [this, id, command]() {
            bool success = false; std::string message;
            try {
                genesis::runtime::RuntimeEvent ev;
                ev.kind = genesis::runtime::RuntimeEventKind::Command;
                ev.label = "agent.delete2d";
                ev.payloadJson = command.dump();
                ev.runtimeHandler = [command, &success, &message](genesis::runtime::Runtime& runtime) {
                    auto& reg = runtime.engine().registry();
                    const auto entId = static_cast<entt::entity>(command.at("entityId").get<std::uint32_t>());
                    if (!reg.valid(entId)) throw std::runtime_error("entity not found");
                    reg.destroy(entId);
                    success = true; message = "deleted";
                };
                (void)runtime_.enqueueEvent(std::move(ev));
            } catch (const std::exception& ex) { message = ex.what(); }
            catch (...) { message = "agent.delete2d unknown error"; }
            completeCommand(id, success, std::move(message));
        });
        {
            std::lock_guard lock(asyncMutex_);
            asyncTasks_.push_back(std::move(task));
        }
        return id;
    }
    if (action == "resource.consume")
    {
        errorMessage.clear();
        if (!command.contains("interactionId") || !command.at("interactionId").is_number_unsigned()) {
            errorMessage = "'resource.consume' requires interactionId";
            return std::nullopt;
        }
        if (!command.contains("amount") || !command.at("amount").is_number_unsigned()) {
            errorMessage = "'resource.consume' requires amount";
            return std::nullopt;
        }
        const auto payload = command.dump();
        const auto label = command.value("label", std::string{"resource.consume"});
        const auto enqueuedAt = std::chrono::steady_clock::now();
        const auto id = recordPending(nextManualCommandId_.fetch_add(1), genesis::runtime::RuntimeEventKind::Command, label, payload, std::move(source), enqueuedAt);
        purgeFinishedTasks();
        auto task = std::async(std::launch::async, [this, id, command]() {
            bool success = false; std::string message;
            try {
                const auto interId = command.at("interactionId").get<std::uint32_t>();
                const auto amount = command.at("amount").get<std::uint32_t>();
                genesis::runtime::RuntimeEvent ev;
                ev.kind = genesis::runtime::RuntimeEventKind::Command;
                ev.label = "resource.consume";
                ev.payloadJson = command.dump();
                ev.runtimeHandler = [interId, amount, &success, &message](genesis::runtime::Runtime& runtime) {
                    const auto taken = runtime.engine().consumeResource(interId, amount);
                    success = (taken > 0);
                    message = std::string("consumed ") + std::to_string(taken) + "/" + std::to_string(amount);
                };
                (void)runtime_.enqueueEvent(std::move(ev));
            } catch (const std::exception& ex) { message = ex.what(); }
            catch (...) { message = "resource.consume unknown error"; }
            completeCommand(id, success, std::move(message));
        });
        {
            std::lock_guard lock(asyncMutex_);
            asyncTasks_.push_back(std::move(task));
        }
        return id;
    }
    if (action == "world.db.save")
    {
        errorMessage.clear();
        if (!command.contains("folder") || !command.at("folder").is_string()) {
            errorMessage = "'world.db.save' requires folder";
            return std::nullopt;
        }
        const auto folder = std::filesystem::path(command.at("folder").get<std::string>());
        const auto payload = command.dump();
        const auto label = command.value("label", std::string{"world.db.save"});
        const auto enqueuedAt = std::chrono::steady_clock::now();
        const auto id = recordPending(nextManualCommandId_.fetch_add(1), genesis::runtime::RuntimeEventKind::Command, label, payload, std::move(source), enqueuedAt);
        purgeFinishedTasks();
        auto task = std::async(std::launch::async, [this, id, folder]() {
            bool success = false; std::string message;
            try {
                auto db = runtime_.worldDatabase();
                if (!db) throw std::runtime_error("no world database loaded");
                auto r = genesis::world::saveWorldDatabaseToFolder(folder, *db);
                success = r.success; message = r.success ? std::string("saved to ")+folder.string() : r.error;
            } catch (const std::exception& ex) { message = ex.what(); }
            catch (...) { message = "world.db.save unknown error"; }
            completeCommand(id, success, std::move(message));
        });
        {
            std::lock_guard lock(asyncMutex_);
            asyncTasks_.push_back(std::move(task));
        }
        return id;
    }
    if (action == "world.db.reload")
    {
        errorMessage.clear();
        if (!command.contains("folder") || !command.at("folder").is_string()) {
            errorMessage = "'world.db.reload' requires folder";
            return std::nullopt;
        }
        const auto folder = std::filesystem::path(command.at("folder").get<std::string>());
        const auto payload = command.dump();
        const auto label = command.value("label", std::string{"world.db.reload"});
        const auto enqueuedAt = std::chrono::steady_clock::now();
        const auto id = recordPending(nextManualCommandId_.fetch_add(1), genesis::runtime::RuntimeEventKind::Command, label, payload, std::move(source), enqueuedAt);
        purgeFinishedTasks();
        auto task = std::async(std::launch::async, [this, id, folder]() {
            bool success = false; std::string message;
            try {
                auto r = runtime_.loadWorldFromFile(folder);
                success = r.success; message = r.success ? std::string("reloaded from ")+folder.string() : r.error;
                if (success) rebuildAtlasOnRuntimeThread();
            } catch (const std::exception& ex) { message = ex.what(); }
            catch (...) { message = "world.db.reload unknown error"; }
            completeCommand(id, success, std::move(message));
        });
        {
            std::lock_guard lock(asyncMutex_);
            asyncTasks_.push_back(std::move(task));
        }
        return id;
    }
    if (action == "agent.stop2d")
    {
        errorMessage.clear();
        if (!command.contains("entityId") || !command.at("entityId").is_number_unsigned()) {
            errorMessage = "'agent.stop2d' requires entityId";
            return std::nullopt;
        }
        const auto payload = command.dump();
        const auto label = command.value("label", std::string{"agent.stop2d"});
        const auto enqueuedAt = std::chrono::steady_clock::now();
        const auto id = recordPending(nextManualCommandId_.fetch_add(1), genesis::runtime::RuntimeEventKind::Command, label, payload, std::move(source), enqueuedAt);
        purgeFinishedTasks();
        auto task = std::async(std::launch::async, [this, id, command]() {
            bool success = false; std::string message;
            try {
                genesis::runtime::RuntimeEvent ev;
                ev.kind = genesis::runtime::RuntimeEventKind::Command;
                ev.label = "agent.stop2d";
                ev.payloadJson = command.dump();
                ev.runtimeHandler = [command, &success, &message](genesis::runtime::Runtime& runtime){
                    auto& reg = runtime.engine().registry();
                    const auto entId = static_cast<entt::entity>(command.at("entityId").get<std::uint32_t>());
                    if (!reg.valid(entId)) throw std::runtime_error("entity not found");
                    if (reg.any_of<genesis::agents::components::MovementIntent2D>(entId)) {
                        reg.remove<genesis::agents::components::MovementIntent2D>(entId);
                    }
                    success = true; message = "stopped";
                };
                (void)runtime_.enqueueEvent(std::move(ev));
            } catch (const std::exception& ex) { message = ex.what(); }
            catch (...) { message = "agent.stop2d unknown error"; }
            completeCommand(id, success, std::move(message));
        });
        {
            std::lock_guard lock(asyncMutex_);
            asyncTasks_.push_back(std::move(task));
        }
        return id;
    }
    if (action == "agent.teleport2d")
    {
        errorMessage.clear();
        if (!command.contains("entityId") || !command.at("entityId").is_number_unsigned()) {
            errorMessage = "'agent.teleport2d' requires entityId";
            return std::nullopt;
        }
        const auto payload = command.dump();
        const auto label = command.value("label", std::string{"agent.teleport2d"});
        const auto enqueuedAt = std::chrono::steady_clock::now();
        const auto id = recordPending(nextManualCommandId_.fetch_add(1), genesis::runtime::RuntimeEventKind::Command, label, payload, std::move(source), enqueuedAt);
        purgeFinishedTasks();
        auto task = std::async(std::launch::async, [this, id, command]() {
            bool success = false; std::string message;
            try {
                genesis::runtime::RuntimeEvent ev;
                ev.kind = genesis::runtime::RuntimeEventKind::Command;
                ev.label = "agent.teleport2d";
                ev.payloadJson = command.dump();
                ev.runtimeHandler = [command, &success, &message](genesis::runtime::Runtime& runtime){
                    auto& reg = runtime.engine().registry();
                    const auto entId = static_cast<entt::entity>(command.at("entityId").get<std::uint32_t>());
                    if (!reg.valid(entId)) throw std::runtime_error("entity not found");
                    auto& loc = reg.get_or_emplace<genesis::agents::components::AgentLocation2D>(entId);
                    loc.mapId = command.value("mapId", loc.mapId);
                    loc.x = command.value("x", loc.x);
                    loc.y = command.value("y", loc.y);
                    if (reg.any_of<genesis::agents::components::MovementIntent2D>(entId)) {
                        reg.remove<genesis::agents::components::MovementIntent2D>(entId);
                    }
                    success = true; message = "teleported";
                };
                (void)runtime_.enqueueEvent(std::move(ev));
            } catch (const std::exception& ex) { message = ex.what(); }
            catch (...) { message = "agent.teleport2d unknown error"; }
            completeCommand(id, success, std::move(message));
        });
        {
            std::lock_guard lock(asyncMutex_);
            asyncTasks_.push_back(std::move(task));
        }
        return id;
    }
    if (action == "world.db.load")
    {
        // Experimental: load new world database (GUI-side only)
        errorMessage.clear();
        if (!command.contains("folder") || !command.at("folder").is_string())
        {
            errorMessage = "'world.db.load' requires folder field";
            return std::nullopt;
        }
        const auto folder = std::filesystem::path(command.at("folder").get<std::string>());

        const auto payload = command.dump();
        const auto label = command.value("label", std::string{"world.db.load"});
        const auto enqueuedAt = std::chrono::steady_clock::now();
        const auto id = recordPending(nextManualCommandId_.fetch_add(1), genesis::runtime::RuntimeEventKind::Command, label, payload, std::move(source), enqueuedAt);

        purgeFinishedTasks();
        auto task = std::async(std::launch::async, [this, id, folder]() {
            std::string message;
            bool success = false;
            try
            {
                if (this->loadWorldDatabaseFolder(folder, message))
                {
                    if (message.empty()) message = "world.db.load succeeded";
                    success = true;
                }
                else
                {
                    if (message.empty()) message = "world.db.load failed";
                }
            }
            catch (const std::exception& ex)
            {
                message = ex.what();
            }
            catch (...)
            {
                message = "world.db.load unknown error";
            }
            completeCommand(id, success, std::move(message));
        });
        {
            std::lock_guard lock(asyncMutex_);
            asyncTasks_.push_back(std::move(task));
        }
        return id;
    }

    errorMessage = "Unsupported command action: " + action;
    return std::nullopt;
}

namespace
{
struct CommandFeedback
{
    std::string message;
};
} // namespace

std::optional<std::uint64_t> RuntimeBridge::enqueueWorldGenerationCommand(const json& descriptor, std::string source, std::string& errorMessage)
{
    errorMessage.clear();
    if (!descriptor.contains("configPath") || !descriptor.at("configPath").is_string())
    {
        errorMessage = "'world.generate' requires configPath field";
        return std::nullopt;
    }

    const auto configPath = std::filesystem::path(descriptor.at("configPath").get<std::string>());
    std::optional<std::uint64_t> seedOverride;
    if (descriptor.contains("seed"))
    {
        if (!descriptor.at("seed").is_number_unsigned())
        {
            errorMessage = "'seed' field must be an unsigned integer";
            return std::nullopt;
        }
        seedOverride = descriptor.at("seed").get<std::uint64_t>();
    }

    std::optional<std::filesystem::path> outputPath;
    if (descriptor.contains("outputPath"))
    {
        if (!descriptor.at("outputPath").is_string())
        {
            errorMessage = "'outputPath' field must be a string";
            return std::nullopt;
        }
        outputPath = std::filesystem::path(descriptor.at("outputPath").get<std::string>());
    }

    const auto payload = descriptor.dump();
    const auto label = descriptor.value("label", std::string{"world.generate"});
    const auto enqueuedAt = std::chrono::steady_clock::now();
    const auto id = recordPending(nextManualCommandId_.fetch_add(1), genesis::runtime::RuntimeEventKind::Command, label, payload, std::move(source), enqueuedAt);

    purgeFinishedTasks();

    auto task = std::async(std::launch::async, [this, id, configPath, seedOverride, outputPath]() {
        bool success = false;
        std::string message;
        try
        {
            auto result = generateWorld(configPath, seedOverride, outputPath);
            if (result && result->success)
            {
                std::ostringstream oss;
                oss << "seed=" << result->seed.value << " locations=" << result->locationCount << " edges=" << result->edgeCount;
                message = oss.str();
                success = true;
                rebuildAtlasOnRuntimeThread();
            }
            else if (result)
            {
                message = result->error.empty() ? "World generation failed" : result->error;
            }
            else
            {
                message = "World generation failed";
            }
        }
        catch (const std::exception& ex)
        {
            message = ex.what();
        }
        catch (...)
        {
            message = "world.generate unknown error";
        }

        if (message.empty())
        {
            message = success ? "world.generate succeeded" : "world.generate failed";
        }

        completeCommand(id, success, std::move(message));
    });

    {
        std::lock_guard lock(asyncMutex_);
        asyncTasks_.push_back(std::move(task));
    }

    return id;
}

std::optional<std::uint64_t> RuntimeBridge::enqueueWorldReloadCommand(const json& descriptor, std::string source, std::string& errorMessage)
{
    errorMessage.clear();
    if (!descriptor.contains("path") || !descriptor.at("path").is_string())
    {
        errorMessage = "'world.load' requires path field";
        return std::nullopt;
    }
    const auto target = std::filesystem::path(descriptor.at("path").get<std::string>());

    auto feedback = std::make_shared<CommandFeedback>();
    const auto payload = descriptor.dump();
    genesis::runtime::RuntimeEvent event;
    event.kind = genesis::runtime::RuntimeEventKind::Command;
    event.label = descriptor.value("label", std::string{"world.load"});
    event.payloadJson = payload;
    event.runtimeHandler = [target, feedback](genesis::runtime::Runtime& runtime) {
        auto result = runtime.loadWorldFromFile(target);
        if (!result.success)
        {
            feedback->message = result.error.empty() ? "World load failed" : result.error;
            throw std::runtime_error(feedback->message);
        }
        feedback->message = "world.load succeeded";
    };
    event.onComplete = [this, feedback](genesis::runtime::RuntimeEventReport& report) {
        if (!feedback->message.empty())
        {
            report.message = feedback->message;
        }
        if (report.success)
        {
            rebuildAtlasOnRuntimeThread();
        }
    };

    const auto id = enqueueRuntimeEvent(std::move(event), std::move(source));
    return id;
}

std::optional<std::uint64_t> RuntimeBridge::enqueueWorldSaveCommand(const json& descriptor, std::string source, std::string& errorMessage)
{
    errorMessage.clear();
    if (!descriptor.contains("path") || !descriptor.at("path").is_string())
    {
        errorMessage = "'world.save' requires path field";
        return std::nullopt;
    }
    const auto target = std::filesystem::path(descriptor.at("path").get<std::string>());

    auto feedback = std::make_shared<CommandFeedback>();
    const auto payload = descriptor.dump();
    genesis::runtime::RuntimeEvent event;
    event.kind = genesis::runtime::RuntimeEventKind::Command;
    event.label = descriptor.value("label", std::string{"world.save"});
    event.payloadJson = payload;
    event.runtimeHandler = [target, feedback](genesis::runtime::Runtime& runtime) {
        auto result = runtime.saveWorldToFile(target);
        if (!result.success)
        {
            feedback->message = result.error.empty() ? "World save failed" : result.error;
            throw std::runtime_error(feedback->message);
        }
        feedback->message = "world.save succeeded";
    };
    event.onComplete = [feedback](genesis::runtime::RuntimeEventReport& report) {
        if (!feedback->message.empty())
        {
            report.message = feedback->message;
        }
    };

    const auto id = enqueueRuntimeEvent(std::move(event), std::move(source));
    return id;
}

void RuntimeBridge::rebuildAtlasOnRuntimeThread()
{
    WorldAtlas nextAtlas{};
    if (auto db = runtime_.worldDatabase()) {
        nextAtlas = buildWorldAtlas(*db);
    } else if (worldDb_) {
        nextAtlas = buildWorldAtlas(*worldDb_);
    } else {
        nextAtlas = {};
    }
    std::lock_guard snapLock(snapshotMutex_);
    atlas_ = std::move(nextAtlas);
    snapshots_.clear();
}

void RuntimeBridge::runLoop()
{
    spdlog::info("RuntimeBridge background loop starting");
    auto idleDelay = std::chrono::milliseconds(2);

    for (;;)
    {
        std::unique_lock lock(controlMutex_);
        controlCv_.wait(lock, [this]() {
            return stopRequested_ || pendingSteps_ > 0 || !paused_;
        });

        if (stopRequested_)
        {
            break;
        }

        const bool paused = paused_;
        std::uint64_t stepsToRun = 0;

        if (paused)
        {
            stepsToRun = std::min<std::uint64_t>(pendingSteps_, 1);
            if (stepsToRun == 0)
            {
                continue;
            }
            pendingSteps_ -= stepsToRun;
        }
        else
        {
            const double speed = speedMultiplier_;
            stepsToRun = std::max<std::uint64_t>(1, static_cast<std::uint64_t>(std::round(speed)));
        }

        lock.unlock();

        runtime_.step(stepsToRun);
        captureSnapshot();

        if (!paused)
        {
            std::this_thread::sleep_for(idleDelay);
        }
    }

    spdlog::info("RuntimeBridge background loop stopping");
}

void RuntimeBridge::captureSnapshot()
{
    const auto* runtimeSnapshot = runtime_.latestSnapshot();
    if (!runtimeSnapshot)
    {
        return;
    }

    RuntimeBridge::Snapshot snapshot;
    snapshot.version = runtimeSnapshot->version;
    snapshot.telemetry = runtimeSnapshot->telemetry;
    snapshot.capturedAt = runtimeSnapshot->capturedAt;
    if (snapshot.capturedAt == std::chrono::steady_clock::time_point{})
    {
        snapshot.capturedAt = std::chrono::steady_clock::now();
    }
    snapshot.events = runtimeSnapshot->events;
    reconcileCommands(snapshot.events);
    advanceSequencesFor(snapshot.events);
    {
        std::lock_guard commandLock(commandMutex_);
        snapshot.executedCommands = snapshot.events;
        snapshot.pendingCommandIds.reserve(pendingCommands_.size());
        for (const auto& [id, _] : pendingCommands_)
        {
            snapshot.pendingCommandIds.push_back(id);
        }
    }
    if (auto diff = runtime_.latestSnapshotDiff())
    {
        snapshot.diff = std::move(*diff);
    }
    else
    {
        snapshot.diff.reset();
    }
    snapshot.agentPositions.reserve(snapshot.telemetry.agents.size());
    for (const auto& agent : snapshot.telemetry.agents)
    {
        // 新语义：优先使用 agent.position（直线移动），不依赖节点坐标
        Vector2 pos{agent.position.x, agent.position.y};
        snapshot.agentPositions.push_back(pos);
    }

    std::lock_guard lock(snapshotMutex_);
    snapshots_.push_back(std::move(snapshot));
    while (snapshots_.size() > maxSnapshots_)
    {
        snapshots_.pop_front();
    }
}

RuntimeBridge::WorldAtlas RuntimeBridge::buildWorldAtlas(const genesis::core::Engine&) {
    // 旧路径已废弃：返回空 Atlas，由 GUI 判断是否可渲染
    return {};
}

} // namespace Genesis::Sandbox::Gui

