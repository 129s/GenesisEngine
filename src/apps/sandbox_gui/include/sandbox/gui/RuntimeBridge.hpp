#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "genesis/runtime/Runtime.hpp"
#include "genesis/runtime/SnapshotDiff.hpp"
#include "genesis/telemetry/TelemetryBuffer.hpp"
#include "genesis/world/WorldDatabase.hpp"

// 前置声明：新世界数据库接口（避免在头文件中包含加载器实现）
namespace genesis { namespace world { class WorldDatabase; } }

namespace genesis::sandbox::gui
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
        std::uint64_t version{0};
        genesis::telemetry::TickTelemetry telemetry;
        std::chrono::steady_clock::time_point capturedAt{};
        std::vector<RuntimeBridge::Vector2> agentPositions;
        std::vector<Genesis::Runtime::RuntimeEventReport> events;
        std::optional<Genesis::Runtime::SimulationSnapshotDiff> diff;
        std::vector<Genesis::Runtime::RuntimeEventReport> executedCommands;
        std::vector<std::uint64_t> pendingCommandIds;
    };

    struct WorldAtlas
    {
        struct Node
        {
            std::uint32_t id{0};
            std::optional<std::uint32_t> parent{};
            std::string name;
            Vector2 position;
        };

        struct Edge
        {
            std::uint32_t from{0};
            std::uint32_t to{0};
            bool bidirectional{true};
            std::vector<Vector2> polyline;
            std::optional<Vector2> anchorFrom;
            std::optional<Vector2> anchorTo;
        };

        struct Portal
        {
            std::uint32_t to{0}; // target node id
            Vector2 anchor;       // local grid coord in owning node's scene
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
            std::string name;
            Genesis::World::ResourceType type{Genesis::World::ResourceType::Food};
            std::uint32_t mapId{0};
            std::uint32_t interactionId{0};
            Vector2 position;
        };

        std::vector<Node> nodes;
        std::vector<Edge> edges;
        std::vector<Spawn> spawns;
        std::unordered_map<std::uint32_t, Vector2> nodeLookup;
        Vector2 extent{800.0f, 600.0f};
        std::vector<Tilemap> tilemaps;

        [[nodiscard]] std::optional<Vector2> nodePosition(std::uint32_t id) const
        {
            auto it = nodeLookup.find(id);
            if (it == nodeLookup.end())
            {
                return std::nullopt;
            }
            return it->second;
        }
    };

    enum class CommandState
    {
        Pending,
        Succeeded,
        Failed
    };

    struct CommandProgress
    {
        std::uint64_t id{0};
        Genesis::Runtime::RuntimeEventKind kind{Genesis::Runtime::RuntimeEventKind::Command};
        std::string label;
        std::optional<std::string> payloadJson;
        std::string source;
        CommandState state{CommandState::Pending};
        std::chrono::steady_clock::time_point enqueuedAt{};
        std::optional<std::chrono::steady_clock::time_point> executedAt;
        std::string message;
    };

    explicit RuntimeBridge(Genesis::Runtime::RuntimeConfig config = {}, std::size_t maxSnapshots = 96);
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

    std::optional<Genesis::Runtime::Runtime::WorldGenerationResult> generateWorld(const std::filesystem::path& configPath, std::optional<std::uint64_t> seedOverride = std::nullopt, std::optional<std::filesystem::path> outputPath = std::nullopt);
    Genesis::World::WorldDbLoadResult loadWorld(const std::filesystem::path& path);
    Genesis::World::WorldDbSaveResult saveWorld(const std::filesystem::path& path);
    [[nodiscard]] std::optional<Genesis::Runtime::Runtime::WorldGenerationResult> lastGeneration() const noexcept;

        [[nodiscard]] std::optional<Snapshot> latestSnapshot() const;

        [[nodiscard]] const WorldAtlas& atlas() const noexcept { return atlas_; }

    std::uint64_t enqueueRuntimeEvent(Genesis::Runtime::RuntimeEvent event, std::string source = "direct");
    std::optional<std::uint64_t> enqueueCommandFromJson(const nlohmann::json& descriptor, std::string source, std::string& errorMessage);
    bool enqueueCommandSequence(const nlohmann::json& script, std::string source, std::string& errorMessage);
    bool enqueueCommandScript(const std::filesystem::path& scriptPath, std::string source, std::string& errorMessage);

    [[nodiscard]] std::vector<CommandProgress> commandStatusSnapshot() const;

    private:
        void runLoop();
        void captureSnapshot();
        static WorldAtlas buildWorldAtlas(const Genesis::World::WorldDatabase& db);
        void reconcileCommands(const std::vector<Genesis::Runtime::RuntimeEventReport>& reports);
        std::uint64_t recordPending(std::uint64_t id, Genesis::Runtime::RuntimeEventKind kind, std::string label, std::optional<std::string> payload, std::string source, std::chrono::steady_clock::time_point enqueuedAt);
        void completeCommand(std::uint64_t id, bool success, std::string message, std::optional<std::string> payloadOverride = std::nullopt);
        void purgeFinishedTasks();

    Genesis::Runtime::Runtime runtime_;
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
    mutable std::mutex lastGenerationMutex_;
    std::optional<Genesis::Runtime::Runtime::WorldGenerationResult> lastGeneration_;

    mutable std::mutex commandMutex_;
    std::unordered_map<std::uint64_t, CommandProgress> pendingCommands_;
    std::deque<CommandProgress> commandHistory_;
    std::size_t maxCommandHistory_{128};
    std::atomic<std::uint64_t> nextManualCommandId_{1'000'000'000ULL};

    struct ScriptCommand
    {
        nlohmann::json descriptor;
        bool waitForSuccess{false};
    };

    struct CommandSequence
    {
        std::string name;
        std::string source;
        std::vector<ScriptCommand> commands;
        std::size_t nextIndex{0};
        std::optional<std::uint64_t> waitingOn;
        bool aborted{false};
        std::string errorMessage;
    };

    using CommandSequencePtr = std::shared_ptr<CommandSequence>;

    void advanceSequencesFor(const std::vector<Genesis::Runtime::RuntimeEventReport>& reports);
    bool scheduleSequence(const CommandSequencePtr& sequence, std::string& errorMessage);
    std::optional<std::uint64_t> enqueueCommandInternal(const nlohmann::json& command, std::string source, std::string& errorMessage);
    void rebuildAtlasOnRuntimeThread();

    mutable std::mutex sequenceMutex_;
    std::deque<CommandSequencePtr> sequences_;
    std::mutex asyncMutex_;
    std::vector<std::future<void>> asyncTasks_;

    std::optional<std::uint64_t> enqueueWorldGenerationCommand(const nlohmann::json& descriptor, std::string source, std::string& errorMessage);
    std::optional<std::uint64_t> enqueueWorldReloadCommand(const nlohmann::json& descriptor, std::string source, std::string& errorMessage);
    std::optional<std::uint64_t> enqueueWorldSaveCommand(const nlohmann::json& descriptor, std::string source, std::string& errorMessage);
};

} // namespace genesis::sandbox::gui
