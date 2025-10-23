#include "sandbox/gui/RuntimeBridge.hpp"
#include "genesis/core/Engine.hpp"

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

} // namespace

RuntimeBridge::RuntimeBridge(genesis::runtime::RuntimeConfig config, std::size_t maxSnapshots)
    : runtime_(std::move(config))
    , maxSnapshots_(std::max<std::size_t>(1, maxSnapshots))
    , atlas_(buildWorldAtlas(runtime_.engine()))
    , maxCommandHistory_(kDefaultCommandHistory)
{
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
    auto nextAtlas = buildWorldAtlas(runtime_.engine());
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
        // Safe: map to static node position only; avoids touching ECS from GUI thread.
        auto pos = atlas_.nodePosition(agent.location).value_or(Vector2{});
        snapshot.agentPositions.push_back(pos);
    }

    std::lock_guard lock(snapshotMutex_);
    snapshots_.push_back(std::move(snapshot));
    while (snapshots_.size() > maxSnapshots_)
    {
        snapshots_.pop_front();
    }
}

RuntimeBridge::WorldAtlas RuntimeBridge::buildWorldAtlas(const genesis::core::Engine& engine)
{
    RuntimeBridge::WorldAtlas atlas;

    const auto nodes = engine.world().locations();
    if (nodes.empty())
    {
        return atlas;
    }
    // Compute positions from global grid coordinates if provided; otherwise simple layered fallback layout.
    bool hasAllGlobal = true;
    int minX = std::numeric_limits<int>::max();
    int minY = std::numeric_limits<int>::max();
    int maxX = std::numeric_limits<int>::min();
    int maxY = std::numeric_limits<int>::min();
    for (const auto& node : nodes)
    {
        if (!node.coord_global.has_value())
        {
            hasAllGlobal = false;
            break;
        }
        minX = std::min(minX, node.coord_global->first);
        minY = std::min(minY, node.coord_global->second);
        maxX = std::max(maxX, node.coord_global->first);
        maxY = std::max(maxY, node.coord_global->second);
    }

    std::unordered_map<genesis::world::LocationId, Vector2, genesis::world::LocationIdHasher> positions;
    positions.reserve(nodes.size());

    if (!hasAllGlobal)
    {
        spdlog::error("WorldAtlas requires coord_global for all nodes under the new schema; map rendering will be empty.");
        // Leave atlas.nodes empty to signal UI there is no drawable map; positions remain empty.
        return atlas;
    }

    const float width = static_cast<float>(std::max(1, maxX - minX + 1));
    const float height = static_cast<float>(std::max(1, maxY - minY + 1));
    atlas.extent = Vector2{std::max(width, 1.0f), std::max(height, 1.0f)};

    for (const auto& node : nodes)
    {
        const int gx = node.coord_global->first - minX;
        const int gy = node.coord_global->second - minY;
        Vector2 position{static_cast<float>(gx), static_cast<float>(gy)};
        positions.emplace(node.id, position);
        atlas.nodes.push_back(WorldAtlas::Node{
            .id = node.id,
            .parent = node.parent,
            .kind = node.kind,
            .name = node.name,
            .position = position,
        });
        atlas.nodeLookup.emplace(node.id.value, position);
    }

    std::set<std::pair<std::uint32_t, std::uint32_t>> seenEdges;
    for (const auto& node : nodes)
    {
        const auto edges = engine.world().edgesFrom(node.id);
        for (const auto& edge : edges)
        {
            const auto key = std::minmax(edge.from.value, edge.to.value);
            if (!seenEdges.insert(key).second)
            {
                continue;
            }

            WorldAtlas::Edge e{};
            e.from = edge.from;
            e.to = edge.to;
            e.bidirectional = edge.bidirectional;
            // Map polyline from grid ints to atlas Vector2 (use raw grid units)
            if (!edge.polyline.empty())
            {
                e.polyline.reserve(edge.polyline.size());
                for (const auto& pt : edge.polyline)
                {
                    e.polyline.push_back(Vector2{static_cast<float>(pt.first), static_cast<float>(pt.second)});
                }
            }
            if (edge.anchor_at_from.has_value())
            {
                e.anchorFrom = Vector2{static_cast<float>(edge.anchor_at_from->first), static_cast<float>(edge.anchor_at_from->second)};
            }
            if (edge.anchor_at_to.has_value())
            {
                e.anchorTo = Vector2{static_cast<float>(edge.anchor_at_to->first), static_cast<float>(edge.anchor_at_to->second)};
            }
            atlas.edges.push_back(std::move(e));
        }
    }

    for (const auto& spawn : engine.world().allSpawns())
    {
        Vector2 position{};
        if (auto it = positions.find(spawn.location); it != positions.end())
        {
            position = it->second;
        }

        atlas.spawns.push_back(WorldAtlas::Spawn{
            .resource = spawn,
            .position = position,
        });
    }

    // Build tilemap metadata: start with explicit meta from world, then add portals and infer bounds
    std::unordered_map<std::uint32_t, WorldAtlas::Tilemap> tilemapByNode;
    for (const auto& tm : engine.world().tilemaps())
    {
        WorldAtlas::Tilemap t{};
        t.nodeId = tm.node.value;
        t.width = tm.width;
        t.height = tm.height;
        t.tileW = tm.tileW;
        t.tileH = tm.tileH;
        tilemapByNode[tm.node.value] = std::move(t);
    }
    // Portals from edges
    for (const auto& e : atlas.edges)
    {
        if (e.anchorFrom.has_value())
        {
            auto& tm = tilemapByNode[e.from.value];
            tm.nodeId = e.from.value;
            tm.portals.push_back(WorldAtlas::Portal{.to = e.to, .anchor = *e.anchorFrom});
        }
        if (e.anchorTo.has_value())
        {
            auto& tm = tilemapByNode[e.to.value];
            tm.nodeId = e.to.value;
            tm.portals.push_back(WorldAtlas::Portal{.to = e.from, .anchor = *e.anchorTo});
        }
    }
    // Bounds from spawns local coords
    struct Bounds { int minx{INT_MAX}, miny{INT_MAX}, maxx{INT_MIN}, maxy{INT_MIN}; };
    std::unordered_map<std::uint32_t, Bounds> bounds;
    for (const auto& s : atlas.spawns)
    {
        if (!s.resource.local_coord.has_value()) continue;
        auto& b = bounds[s.resource.location.value];
        b.minx = std::min(b.minx, s.resource.local_coord->first);
        b.miny = std::min(b.miny, s.resource.local_coord->second);
        b.maxx = std::max(b.maxx, s.resource.local_coord->first);
        b.maxy = std::max(b.maxy, s.resource.local_coord->second);
    }
    for (auto& [nodeId, tm] : tilemapByNode)
    {
        if (auto itb = bounds.find(nodeId); itb != bounds.end())
        {
            auto b = itb->second;
            if (b.minx <= b.maxx && b.miny <= b.maxy)
            {
                tm.width = std::max(1, b.maxx - b.minx + 1);
                tm.height = std::max(1, b.maxy - b.miny + 1);
            }
        }
        atlas.tilemaps.push_back(std::move(tm));
    }

    return atlas;
}

} // namespace Genesis::Sandbox::Gui

