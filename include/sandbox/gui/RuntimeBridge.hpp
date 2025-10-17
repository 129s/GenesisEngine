#pragma once

#include <chrono>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <memory>
#include <optional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "genesis/runtime/Runtime.hpp"
#include "genesis/core/Engine.hpp"
#include "genesis/telemetry/TelemetryBuffer.hpp"
#include "genesis/world/WorldRegistry.hpp"

namespace Genesis::Sandbox::Gui
{

class RuntimeBridge
{
public:
    struct Vector2
    {
        float x{0.0f};
        float y{0.0f};
    };

    struct Snapshot
    {
        genesis::telemetry::TickTelemetry telemetry;
        std::chrono::steady_clock::time_point capturedAt{};
    };

    struct WorldAtlas
    {
        struct Node
        {
            genesis::world::LocationId id{};
            genesis::world::LocationId parent{};
            genesis::world::LocationKind kind{genesis::world::LocationKind::Point};
            std::string name;
            Vector2 position;
        };

        struct Edge
        {
            genesis::world::LocationId from{};
            genesis::world::LocationId to{};
            bool bidirectional{true};
        };

        struct Spawn
        {
            genesis::world::ResourceSpawn resource;
            Vector2 position;
        };

        std::vector<Node> nodes;
        std::vector<Edge> edges;
        std::vector<Spawn> spawns;
        Vector2 extent{800.0f, 600.0f};
    };

    explicit RuntimeBridge(genesis::runtime::RuntimeConfig config = {}, std::size_t maxSnapshots = 96);
    ~RuntimeBridge();

    RuntimeBridge(const RuntimeBridge&) = delete;
    RuntimeBridge& operator=(const RuntimeBridge&) = delete;
    RuntimeBridge(RuntimeBridge&&) = delete;
    RuntimeBridge& operator=(RuntimeBridge&&) = delete;

    bool start();
    void stop();

    void setPaused(bool paused);
    [[nodiscard]] bool paused() const;

    void requestStep(std::uint64_t steps = 1);

    void setSpeedMultiplier(double multiplier);
    [[nodiscard]] double speedMultiplier() const;

    [[nodiscard]] std::optional<Snapshot> latestSnapshot() const;

    [[nodiscard]] const WorldAtlas& atlas() const noexcept { return atlas_; }

private:
    void runLoop();
    void captureSnapshot();
    static WorldAtlas buildWorldAtlas(const genesis::core::Engine& engine);

    genesis::runtime::Runtime runtime_;
    std::size_t maxSnapshots_;

    mutable std::mutex snapshotMutex_;
    std::deque<Snapshot> snapshots_;

    mutable std::mutex controlMutex_;
    std::condition_variable controlCv_;
    bool running_{false};
    bool stopRequested_{false};
    bool paused_{false};
    std::uint64_t pendingSteps_{0};
    double speedMultiplier_{1.0};
    std::thread worker_;

    WorldAtlas atlas_;
};

} // namespace Genesis::Sandbox::Gui
