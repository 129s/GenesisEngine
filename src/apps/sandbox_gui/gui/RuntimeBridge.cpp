#include "sandbox/gui/RuntimeBridge.hpp"
#include "genesis/world/WorldDatabaseLoader.hpp"
#include "genesis/world/WorldDatabaseSaver.hpp"
#include "genesis/agents/Movement2D.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <climits>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

// Note: avoid including ECS headers here to prevent accidental
// cross-thread access from GUI thread.

namespace genesis::sandbox::gui
{
namespace
{
constexpr std::size_t kDefaultCommandHistory = 128;
} // namespace

RuntimeBridge::RuntimeBridge(Genesis::Runtime::RuntimeConfig config, std::size_t maxSnapshots)
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

std::optional<Genesis::Runtime::Runtime::WorldGenerationResult> RuntimeBridge::generateWorld(const std::filesystem::path& configPath, std::optional<std::uint64_t> seedOverride, std::optional<std::filesystem::path> outputPath)
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

Genesis::World::WorldDbLoadResult RuntimeBridge::loadWorld(const std::filesystem::path& path)
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
        if (auto db = runtime_.worldDatabase()) {
            atlas_ = buildWorldAtlas(*db);
        } else {
            atlas_ = {};
        }
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

Genesis::World::WorldDbSaveResult RuntimeBridge::saveWorld(const std::filesystem::path& path)
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

std::optional<Genesis::Runtime::Runtime::WorldGenerationResult> RuntimeBridge::lastGeneration() const noexcept
{
    std::lock_guard lock(lastGenerationMutex_);
    return lastGeneration_;
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

} // namespace genesis::sandbox::gui

