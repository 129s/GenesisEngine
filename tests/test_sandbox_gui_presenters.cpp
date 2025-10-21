#include <gtest/gtest.h>

#include <cstring>

#include "sandbox/gui/presenter/MainViewPresenters.hpp"

using namespace Genesis::Sandbox::Gui;

namespace
{

RuntimeBridge::WorldAtlas makeAtlas()
{
    RuntimeBridge::WorldAtlas atlas;
    RuntimeBridge::WorldAtlas::Node nodeA;
    nodeA.id = genesis::world::LocationId{1};
    nodeA.name = "Root";
    nodeA.position = {0.0f, 0.0f};
    atlas.nodes.push_back(nodeA);

    RuntimeBridge::WorldAtlas::Node nodeB;
    nodeB.id = genesis::world::LocationId{2};
    nodeB.name = "Child";
    nodeB.position = {10.0f, 5.0f};
    atlas.nodes.push_back(nodeB);

    atlas.nodeLookup.emplace(1, nodeA.position);
    atlas.nodeLookup.emplace(2, nodeB.position);
    atlas.extent = {20.0f, 20.0f};

    RuntimeBridge::WorldAtlas::Edge edge;
    edge.from = nodeA.id;
    edge.to = nodeB.id;
    edge.anchorFrom = RuntimeBridge::Vector2{2.0f, 1.0f};
    edge.anchorTo = RuntimeBridge::Vector2{4.0f, 3.0f};
    atlas.edges.push_back(edge);

    RuntimeBridge::WorldAtlas::Spawn spawn;
    spawn.resource.location = nodeA.id;
    spawn.resource.local_coord = std::make_pair(3, 4);
    spawn.resource.type = genesis::world::ResourceType::Food;
    atlas.spawns.push_back(spawn);

    RuntimeBridge::WorldAtlas::Tilemap tilemap;
    tilemap.nodeId = nodeA.id.value;
    tilemap.width = 4;
    tilemap.height = 3;
    tilemap.tileW = 16;
    tilemap.tileH = 16;
    atlas.tilemaps.push_back(tilemap);

    return atlas;
}

RuntimeBridge::Snapshot makeSnapshot()
{
    RuntimeBridge::Snapshot snapshot;
    snapshot.telemetry.step = 42;

    genesis::telemetry::ResourceSnapshot res;
    res.location = genesis::world::LocationId{1};
    res.name = "Food";
    res.current = 6;
    res.capacity = 10;
    res.type = genesis::world::ResourceType::Food;
    snapshot.telemetry.resources.push_back(res);

    genesis::telemetry::ActionSnapshot action;
    action.entityId = 7;
    action.currentAction = "ConsumeResource";
    snapshot.telemetry.actions.push_back(action);

    genesis::telemetry::AgentSnapshot agent;
    agent.entityId = 7;
    agent.name = "Agent007";
    agent.location = genesis::world::LocationId{1};
    snapshot.telemetry.agents.push_back(agent);

    genesis::telemetry::MovementProgressSnapshot progress;
    progress.entityId = 7;
    progress.from = genesis::world::LocationId{1};
    progress.to = genesis::world::LocationId{2};
    progress.t01 = 0.5f;
    snapshot.telemetry.movementProgress.push_back(progress);

    genesis::telemetry::NeedSnapshot need;
    need.value = 0.25f;
    need.critical = true;
    snapshot.telemetry.needs.push_back(need);

    return snapshot;
}

} // namespace

TEST(ScenePresenterTests, MapViewAggregatesResourcesAndActivities)
{
    UiState state;
    RuntimeBridge::WorldAtlas atlas = makeAtlas();
    RuntimeBridge::Snapshot snapshot = makeSnapshot();

    ScenePresenterInput input{
        state,
        &atlas,
        &snapshot};

    ScenePresenter presenter;
    const SceneMapViewModel vm = presenter.buildMapViewModel(input);

    ASSERT_TRUE(vm.runtimeReady);
    ASSERT_TRUE(vm.hasSnapshot);
    ASSERT_EQ(vm.resourceBuckets.size(), 1u);
    EXPECT_EQ(vm.resourceBuckets.front().locationId, 1u);
    ASSERT_EQ(vm.resourceBuckets.front().resources.size(), 1u);
    EXPECT_EQ(vm.resourceBuckets.front().resources.front().name, "Food");

    auto movementIt = vm.movement.find(7u);
    ASSERT_NE(movementIt, vm.movement.end());
    EXPECT_FLOAT_EQ(movementIt->second.t, 0.5f);

    auto agentIt = vm.agents.find(7u);
    ASSERT_NE(agentIt, vm.agents.end());
    EXPECT_EQ(agentIt->second.activity, SceneAgentActivity::Consume);
}

TEST(ScenePresenterTests, NodeViewProvidesTilemapAndAnchors)
{
    UiState state;
    RuntimeBridge::WorldAtlas atlas = makeAtlas();
    state.scene_selected_node = 1;

    ScenePresenterInput input{
        state,
        &atlas,
        nullptr};

    ScenePresenter presenter;
    const SceneNodeViewModel vm = presenter.buildNodeViewModel(input);

    ASSERT_TRUE(vm.active.has_value());
    const SceneNodeDetails& details = *vm.active;
    EXPECT_EQ(details.nodeId, 1u);
    EXPECT_EQ(details.resources.size(), 1u);
    EXPECT_TRUE(details.tilemap.has_value());
    EXPECT_EQ(details.tilemap->width, 4);
    EXPECT_EQ(details.anchors.size(), 1u);
    EXPECT_EQ(details.anchors.front().targetNodeId, 2u);
    EXPECT_EQ(details.grid.minX, 0);
    EXPECT_EQ(details.grid.maxX, 3);
}

TEST(WorldPresenterTests, WorldViewModelSummarizesCommands)
{
    UiState state;
    std::snprintf(state.worldgen_config_buffer.data(), state.worldgen_config_buffer.size(), "%s", "config.toml");
    state.world_command_status = "pending";

    RuntimeBridge::CommandProgress command;
    command.id = 5;
    command.label = "generate";
    command.state = RuntimeBridge::CommandState::Succeeded;
    command.message = "done";

    std::vector<RuntimeBridge::CommandProgress> commands;
    commands.push_back(command);

    WorldPresenterInput input{
        state,
        nullptr,
        &commands};

    WorldPresenter presenter;
    const WorldViewModel vm = presenter.buildWorldViewModel(input);

    EXPECT_TRUE(vm.hasConfigPath);
    ASSERT_EQ(vm.commands.size(), 1u);
    EXPECT_NE(vm.commands.front().summary.find("Succeeded"), std::string::npos);
    EXPECT_EQ(vm.worldCommandStatus, "pending");
}

TEST(MonitorPresenterTests, TelemetryAveragesNeeds)
{
    RuntimeBridge::Snapshot snapshot = makeSnapshot();
    MonitorPresenterInput input{&snapshot};

    MonitorPresenter presenter;
    const MonitorTelemetryViewModel vm = presenter.buildTelemetryViewModel(input);

    EXPECT_TRUE(vm.hasSnapshot);
    EXPECT_EQ(vm.agentCount, 1u);
    EXPECT_EQ(vm.actionCount, 1u);
    EXPECT_EQ(vm.needCount, 1u);
    EXPECT_FLOAT_EQ(vm.averageNeed, 0.25f);
    EXPECT_EQ(vm.criticalNeedCount, 1u);
    ASSERT_EQ(vm.resources.size(), 1u);
    EXPECT_EQ(vm.resources.front().locationId, 1u);
}

