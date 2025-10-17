#include "sandbox/gui/RuntimeBridge.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>

#include <spdlog/spdlog.h>

namespace Genesis::Sandbox::Gui
{
namespace
{
constexpr float kHorizontalSpacing = 180.0f;
constexpr float kVerticalSpacing = 140.0f;
constexpr float kMinExtent = 100.0f;

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
{
}

RuntimeBridge::~RuntimeBridge()
{
    stop();
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

std::optional<RuntimeBridge::Snapshot> RuntimeBridge::latestSnapshot() const
{
    std::lock_guard lock(snapshotMutex_);
    if (snapshots_.empty())
    {
        return std::nullopt;
    }
    return snapshots_.back();
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
    const auto* telemetry = runtime_.latestSnapshot();
    if (!telemetry)
    {
        return;
    }

    RuntimeBridge::Snapshot snapshot{
        .telemetry = *telemetry,
        .capturedAt = std::chrono::steady_clock::now(),
    };

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

    std::unordered_map<genesis::world::LocationId, genesis::world::LocationNode, genesis::world::LocationIdHasher> nodeLookup;
    nodeLookup.reserve(nodes.size());
    for (const auto& node : nodes)
    {
        nodeLookup.emplace(node.id, node);
    }

    std::unordered_map<genesis::world::LocationId, int, genesis::world::LocationIdHasher> depthTable;
    depthTable.reserve(nodes.size());

    std::function<int(genesis::world::LocationId)> computeDepth = [&](genesis::world::LocationId id) -> int {
        if (id == genesis::world::InvalidLocation)
        {
            return 0;
        }
        if (auto it = depthTable.find(id); it != depthTable.end())
        {
            return it->second;
        }
        int depth = 0;
        if (auto itNode = nodeLookup.find(id); itNode != nodeLookup.end())
        {
            depth = 1 + computeDepth(itNode->second.parent);
        }
        depthTable.emplace(id, depth);
        return depth;
    };

    std::map<int, std::vector<genesis::world::LocationNode>> levels;
    for (const auto& node : nodes)
    {
        const int depth = computeDepth(node.id);
        levels[depth].push_back(node);
    }

    std::size_t maxPerLevel = 0;
    for (auto& [level, group] : levels)
    {
        std::sort(group.begin(), group.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.id.value < rhs.id.value;
        });
        maxPerLevel = std::max(maxPerLevel, group.size());
    }

    atlas.extent = computeExtent(maxPerLevel, levels.size());

    std::unordered_map<genesis::world::LocationId, Vector2, genesis::world::LocationIdHasher> positions;
    positions.reserve(nodes.size());

    std::size_t levelIndex = 0;
    for (auto& [level, group] : levels)
    {
        const float y = static_cast<float>(levelIndex) * kVerticalSpacing;
        const std::size_t count = group.size();
        const float width = atlas.extent.x;

        for (std::size_t index = 0; index < count; ++index)
        {
            const float x = (static_cast<float>(index + 1) * width) / static_cast<float>(count + 1);
            Vector2 position{x, y};
            positions.emplace(group[index].id, position);

            atlas.nodes.push_back(WorldAtlas::Node{
                .id = group[index].id,
                .parent = group[index].parent,
                .kind = group[index].kind,
                .name = group[index].name,
                .position = position,
            });
            atlas.nodeLookup.emplace(group[index].id.value, position);
        }

        ++levelIndex;
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

            atlas.edges.push_back(WorldAtlas::Edge{
                .from = edge.from,
                .to = edge.to,
                .bidirectional = edge.bidirectional,
            });
        }
    }

    for (const auto& spawn : engine.world().allSpawns())
    {
        Vector2 position{};
        if (auto it = positions.find(spawn.location); it != positions.end())
        {
            position = it->second;
            position.y += 24.0f;
        }

        atlas.spawns.push_back(WorldAtlas::Spawn{
            .resource = spawn,
            .position = position,
        });
    }

    return atlas;
}

} // namespace Genesis::Sandbox::Gui
