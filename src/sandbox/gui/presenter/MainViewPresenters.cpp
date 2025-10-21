#include "sandbox/gui/presenter/MainViewPresenters.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "../CommandUiHelpers.hpp"

namespace Genesis::Sandbox::Gui
{

SceneMapViewModel ScenePresenter::buildMapViewModel(const ScenePresenterInput& input) const
{
    SceneMapViewModel viewModel{};
    viewModel.runtimeReady = (input.atlas != nullptr);
    viewModel.atlas = input.atlas;
    viewModel.hasSnapshot = (input.snapshot != nullptr);

    if (!input.snapshot)
    {
        return viewModel;
    }

    // Group resources by location for overlay rendering.
    std::unordered_map<std::uint32_t, std::vector<SceneMapResource>> groupedResources;
    for (const auto& resource : input.snapshot->telemetry.resources)
    {
        SceneMapResource entry{
            resource.name,
            resource.current,
            resource.capacity,
            resource.type};
        groupedResources[resource.location.value].push_back(std::move(entry));
    }

    viewModel.resourceBuckets.reserve(groupedResources.size());
    for (auto& [locationId, resources] : groupedResources)
    {
        std::sort(resources.begin(), resources.end(), [](const SceneMapResource& lhs, const SceneMapResource& rhs) {
            if (lhs.name == rhs.name)
            {
                return lhs.current > rhs.current;
            }
            return lhs.name < rhs.name;
        });
        viewModel.resourceBuckets.push_back(SceneMapResourceBucket{
            locationId,
            std::move(resources)});
    }
    std::sort(viewModel.resourceBuckets.begin(), viewModel.resourceBuckets.end(), [](const SceneMapResourceBucket& lhs, const SceneMapResourceBucket& rhs) {
        return lhs.locationId < rhs.locationId;
    });

    // Build movement interpolation data.
    if (!input.snapshot->telemetry.movementProgress.empty())
    {
        for (const auto& progress : input.snapshot->telemetry.movementProgress)
        {
            SceneMovementProgress entry{};
            if (input.atlas)
            {
                entry.from = input.atlas->nodePosition(progress.from).value_or(RuntimeBridge::Vector2{});
                entry.to = input.atlas->nodePosition(progress.to).value_or(entry.from);
            }
            else
            {
                entry.from = RuntimeBridge::Vector2{};
                entry.to = RuntimeBridge::Vector2{};
            }
            entry.t = std::clamp(progress.t01, 0.0f, 1.0f);
            viewModel.movement.emplace(progress.entityId, entry);
        }
    }

    // Map agent current activity from telemetry.
    std::unordered_map<std::uint32_t, SceneAgentActivity> activityByEntity;
    activityByEntity.reserve(input.snapshot->telemetry.actions.size());
    for (const auto& action : input.snapshot->telemetry.actions)
    {
        SceneAgentActivity activity = SceneAgentActivity::Other;
        if (action.currentAction == "MoveTo")
        {
            activity = SceneAgentActivity::Move;
        }
        else if (action.currentAction == "ConsumeResource")
        {
            activity = SceneAgentActivity::Consume;
        }
        else if (action.currentAction == "Idle")
        {
            activity = SceneAgentActivity::Idle;
        }
        activityByEntity[action.entityId] = activity;
    }

    for (const auto& agent : input.snapshot->telemetry.agents)
    {
        SceneAgentStatus status{};
        status.entityId = agent.entityId;
        status.locationId = agent.location.value;
        status.name = agent.name;
        if (auto it = activityByEntity.find(agent.entityId); it != activityByEntity.end())
        {
            status.activity = it->second;
        }
        else
        {
            status.activity = SceneAgentActivity::Idle;
        }
        viewModel.agents.emplace(agent.entityId, std::move(status));
    }

    return viewModel;
}

SceneNodeViewModel ScenePresenter::buildNodeViewModel(const ScenePresenterInput& input) const
{
    SceneNodeViewModel viewModel{};

    if (!input.atlas)
    {
        return viewModel;
    }

    viewModel.nodes.reserve(input.atlas->nodes.size());
    for (const auto& node : input.atlas->nodes)
    {
        viewModel.nodes.push_back(SceneNodeSummary{node.id.value, node.name});
    }

    const std::uint32_t selectedId = input.state.scene_selected_node;
    auto findNode = [&]() -> const RuntimeBridge::WorldAtlas::Node* {
        for (const auto& node : input.atlas->nodes)
        {
            if (node.id.value == selectedId)
            {
                return &node;
            }
        }
        return nullptr;
    };

    const auto* selectedNode = findNode();
    if (!selectedNode)
    {
        return viewModel;
    }

    SceneNodeDetails details{};
    details.nodeId = selectedNode->id.value;
    details.name = selectedNode->name;

    // Collect resources with local coordinates.
    for (const auto& spawn : input.atlas->spawns)
    {
        if (spawn.resource.location.value != details.nodeId)
        {
            continue;
        }
        if (!spawn.resource.local_coord.has_value())
        {
            continue;
        }
        const auto& [gx, gy] = *spawn.resource.local_coord;
        details.resources.push_back(SceneNodeResource{
            static_cast<float>(gx),
            static_cast<float>(gy),
            spawn.resource.type});
    }

    // Collect anchors pointing away from this node.
    for (const auto& edge : input.atlas->edges)
    {
        if (edge.from.value == details.nodeId && edge.anchorFrom.has_value())
        {
            details.anchors.push_back(SceneNodeAnchor{
                edge.anchorFrom->x,
                edge.anchorFrom->y,
                edge.to.value});
        }
        if (edge.to.value == details.nodeId && edge.anchorTo.has_value())
        {
            details.anchors.push_back(SceneNodeAnchor{
                edge.anchorTo->x,
                edge.anchorTo->y,
                edge.from.value});
        }
    }

    // Determine tilemap meta if available.
    std::optional<SceneNodeTilemapInfo> tilemapInfo;
    for (const auto& tilemap : input.atlas->tilemaps)
    {
        if (tilemap.nodeId != details.nodeId)
        {
            continue;
        }
        if (tilemap.width <= 0 || tilemap.height <= 0)
        {
            continue;
        }
        tilemapInfo = SceneNodeTilemapInfo{
            tilemap.width,
            tilemap.height,
            std::max(tilemap.tileW, 1),
            std::max(tilemap.tileH, 1)};

        details.portals.reserve(tilemap.portals.size());
        for (const auto& portal : tilemap.portals)
        {
            details.portals.push_back(SceneNodePortal{
                portal.anchor.x,
                portal.anchor.y,
                portal.to.value});
        }
        break;
    }

    // Compute grid bounds.
    SceneNodeGridInfo grid{};
    if (tilemapInfo)
    {
        grid.minX = 0;
        grid.minY = 0;
        grid.maxX = tilemapInfo->width - 1;
        grid.maxY = tilemapInfo->height - 1;
        grid.baseTileSize = static_cast<float>(std::max(tilemapInfo->tileWidth, 1));
    }
    else
    {
        int minX = 0;
        int minY = 0;
        int maxX = 9;
        int maxY = 9;
        auto incorporate = [&](float gx, float gy) {
            minX = std::min(minX, static_cast<int>(std::floor(gx)));
            minY = std::min(minY, static_cast<int>(std::floor(gy)));
            maxX = std::max(maxX, static_cast<int>(std::ceil(gx)));
            maxY = std::max(maxY, static_cast<int>(std::ceil(gy)));
        };

        for (const auto& resource : details.resources)
        {
            incorporate(resource.x, resource.y);
        }
        for (const auto& anchor : details.anchors)
        {
            incorporate(anchor.x, anchor.y);
        }

        grid.minX = minX;
        grid.minY = minY;
        grid.maxX = maxX;
        grid.maxY = maxY;
        grid.baseTileSize = 24.0f;
    }

    details.grid = grid;
    details.tilemap = tilemapInfo;
    viewModel.active = std::move(details);

    return viewModel;
}

WorldViewModel WorldPresenter::buildWorldViewModel(const WorldPresenterInput& input) const
{
    WorldViewModel viewModel{};
    viewModel.runtimeReady = (input.runtime != nullptr);

    if (input.commands)
    {
        viewModel.commands.reserve(input.commands->size());
        for (const auto& command : *input.commands)
        {
            WorldCommandViewModel item{};
            item.id = command.id;
            item.label = command.label;
            item.state = command.state;
            item.summary = commandStateSummary(command);
            item.source = command.source;
            item.message = command.message;
            item.payloadJson = command.payloadJson;
            viewModel.commands.push_back(std::move(item));
        }
    }

    viewModel.worldCommandStatus = input.state.world_command_status;
    viewModel.worldLoadStatus = input.state.world_load_status;
    viewModel.worldSaveStatus = input.state.world_save_status;
    viewModel.commandScriptStatus = input.state.command_script_status;

    viewModel.hasConfigPath = !std::string(input.state.worldgen_config_buffer.data()).empty();
    viewModel.hasOutputPath = !std::string(input.state.worldgen_output_buffer.data()).empty();

    return viewModel;
}

MonitorTelemetryViewModel MonitorPresenter::buildTelemetryViewModel(const MonitorPresenterInput& input) const
{
    MonitorTelemetryViewModel viewModel{};
    if (!input.snapshot)
    {
        return viewModel;
    }

    viewModel.hasSnapshot = true;
    const auto& telemetry = input.snapshot->telemetry;
    viewModel.step = telemetry.step;
    viewModel.agentCount = telemetry.agents.size();
    viewModel.actionCount = telemetry.actions.size();
    viewModel.needCount = telemetry.needs.size();

    if (!telemetry.needs.empty())
    {
        float sum = 0.0f;
        std::uint32_t critical = 0;
        for (const auto& need : telemetry.needs)
        {
            sum += need.value;
            if (need.critical)
            {
                ++critical;
            }
        }
        viewModel.averageNeed = sum / static_cast<float>(telemetry.needs.size());
        viewModel.criticalNeedCount = critical;
    }

    viewModel.resources.reserve(telemetry.resources.size());
    for (const auto& resource : telemetry.resources)
    {
        viewModel.resources.push_back(MonitorResourceViewModel{
            resource.location.value,
            resource.name,
            resource.current,
            resource.capacity});
    }

    return viewModel;
}

} // namespace Genesis::Sandbox::Gui
