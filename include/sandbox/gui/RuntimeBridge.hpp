#pragma once

#include <chrono>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
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
        std::vector<RuntimeBridge::Vector2> agentPositions;
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
            // Optional geometry and anchors (grid-based to be mapped as needed)
            std::vector<Vector2> polyline;
            std::optional<Vector2> anchorFrom;
            std::optional<Vector2> anchorTo;
        };

        struct Portal
        {
            genesis::world::LocationId to{}; // target node id
            Vector2 anchor;                  // local grid coord in owning node's scene
        };

        struct Tilemap
        {
            std::uint32_t nodeId{0};
            int width{0};   // in tiles (0 means unknown)
            int height{0};  // in tiles
            int tileW{1};   // tile pixel size hint (optional)
            int tileH{1};
            std::vector<Portal> portals;
        };

        struct Spawn
        {
            genesis::world::ResourceSpawn resource;
            Vector2 position;
        };

        std::vector<Node> nodes;
        std::vector<Edge> edges;
        std::vector<Spawn> spawns;
        std::unordered_map<std::uint32_t, Vector2> nodeLookup;
        Vector2 extent{800.0f, 600.0f};
        std::vector<Tilemap> tilemaps;

        [[nodiscard]] std::optional<Vector2> nodePosition(genesis::world::LocationId id) const
        {
            auto it = nodeLookup.find(id.value);
            if (it == nodeLookup.end())
            {
                return std::nullopt;
            }
            return it->second;
        }
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

    std::optional<genesis::runtime::Runtime::WorldGenerationResult> generateWorld(const std::filesystem::path& configPath, std::optional<std::uint64_t> seedOverride = std::nullopt, std::optional<std::filesystem::path> outputPath = std::nullopt);
    genesis::world::WorldLoadResult loadWorld(const std::filesystem::path& path);
    genesis::world::WorldSaveResult saveWorld(const std::filesystem::path& path);
    [[nodiscard]] const std::optional<genesis::runtime::Runtime::WorldGenerationResult>& lastGeneration() const noexcept { return lastGeneration_; }

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
    std::optional<genesis::runtime::Runtime::WorldGenerationResult> lastGeneration_;
};

} // namespace Genesis::Sandbox::Gui
