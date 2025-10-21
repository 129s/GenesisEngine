#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "sandbox/gui/RuntimeBridge.hpp"
#include "sandbox/gui/ui/UiState.hpp"

namespace Genesis::Sandbox::Gui
{

struct ScenePresenterInput
{
    const UiState& state;
    const RuntimeBridge::WorldAtlas* atlas{nullptr};
    const RuntimeBridge::Snapshot* snapshot{nullptr};
};

enum class SceneAgentActivity
{
    Idle,
    Move,
    Consume,
    Other
};

struct SceneMapResource
{
    std::string name;
    std::uint32_t current{0};
    std::uint32_t capacity{0};
    genesis::world::ResourceType type{genesis::world::ResourceType::Food};
};

struct SceneMapResourceBucket
{
    std::uint32_t locationId{0};
    std::vector<SceneMapResource> resources;
};

struct SceneMovementProgress
{
    RuntimeBridge::Vector2 from{};
    RuntimeBridge::Vector2 to{};
    float t{0.0f};
};

struct SceneAgentStatus
{
    std::uint32_t entityId{0};
    std::uint32_t locationId{0};
    SceneAgentActivity activity{SceneAgentActivity::Idle};
    std::string name;
};

struct SceneMapViewModel
{
    bool runtimeReady{false};
    const RuntimeBridge::WorldAtlas* atlas{nullptr};
    std::vector<SceneMapResourceBucket> resourceBuckets;
    std::unordered_map<std::uint32_t, SceneMovementProgress> movement;
    std::unordered_map<std::uint32_t, SceneAgentStatus> agents;
    bool hasSnapshot{false};
};

struct SceneNodeSummary
{
    std::uint32_t id{0};
    std::string name;
};

struct SceneNodeAnchor
{
    float x{0.0f};
    float y{0.0f};
    std::uint32_t targetNodeId{0};
};

struct SceneNodeResource
{
    float x{0.0f};
    float y{0.0f};
    genesis::world::ResourceType type{genesis::world::ResourceType::Food};
};

struct SceneNodeTilemapInfo
{
    int width{0};
    int height{0};
    int tileWidth{1};
    int tileHeight{1};
};

struct SceneNodeGridInfo
{
    int minX{0};
    int minY{0};
    int maxX{0};
    int maxY{0};
    float baseTileSize{24.0f};
};

struct SceneNodeDetails
{
    std::uint32_t nodeId{0};
    std::string name;
    SceneNodeGridInfo grid{};
    std::optional<SceneNodeTilemapInfo> tilemap;
    std::vector<SceneNodeResource> resources;
    std::vector<SceneNodeAnchor> anchors;
};

struct SceneNodeViewModel
{
    std::vector<SceneNodeSummary> nodes;
    std::optional<SceneNodeDetails> active;
};

struct WorldPresenterInput
{
    const UiState& state;
    const RuntimeBridge* runtime{nullptr};
    const std::vector<RuntimeBridge::CommandProgress>* commands{nullptr};
};

struct WorldCommandViewModel
{
    std::uint64_t id{0};
    std::string label;
    RuntimeBridge::CommandState state{RuntimeBridge::CommandState::Pending};
    std::string summary;
    std::string source;
    std::string message;
    std::optional<std::string> payloadJson;
};

struct WorldViewModel
{
    bool runtimeReady{false};
    std::vector<WorldCommandViewModel> commands;
    std::string worldCommandStatus;
    std::string worldLoadStatus;
    std::string worldSaveStatus;
    std::string commandScriptStatus;
    bool hasConfigPath{false};
    bool hasOutputPath{false};
};

struct MonitorPresenterInput
{
    const RuntimeBridge::Snapshot* snapshot{nullptr};
};

struct MonitorResourceViewModel
{
    std::uint32_t locationId{0};
    std::string name;
    std::uint32_t current{0};
    std::uint32_t capacity{0};
};

struct MonitorTelemetryViewModel
{
    bool hasSnapshot{false};
    std::uint64_t step{0};
    std::size_t agentCount{0};
    std::size_t actionCount{0};
    std::size_t needCount{0};
    float averageNeed{0.0f};
    std::uint32_t criticalNeedCount{0};
    std::vector<MonitorResourceViewModel> resources;
};

class ScenePresenter
{
public:
    [[nodiscard]] SceneMapViewModel buildMapViewModel(const ScenePresenterInput& input) const;
    [[nodiscard]] SceneNodeViewModel buildNodeViewModel(const ScenePresenterInput& input) const;
};

class WorldPresenter
{
public:
    [[nodiscard]] WorldViewModel buildWorldViewModel(const WorldPresenterInput& input) const;
};

class MonitorPresenter
{
public:
    [[nodiscard]] MonitorTelemetryViewModel buildTelemetryViewModel(const MonitorPresenterInput& input) const;
};

} // namespace Genesis::Sandbox::Gui

