#include <gtest/gtest.h>

#include <entt/entt.hpp>

#include "genesis/agents/AgentComponents.hpp"
#include "genesis/agents/MovementSystem.hpp"
#include "genesis/world/WorldRegistry.hpp"
#include "genesis/world/WorldTypes.hpp"

using genesis::agents::MovementSystem;
using genesis::agents::components::AgentLocation;
using genesis::agents::components::MovementIntent;
using genesis::world::LocationGraph;
using genesis::world::LocationId;
using genesis::world::LocationKind;
using genesis::world::LocationNode;
using genesis::world::PathEdge;

namespace {

LocationGraph simpleGraph() {
    LocationGraph graph;
    graph.nodes.push_back(LocationNode{LocationId{1}, genesis::world::InvalidLocation, "A", LocationKind::Point, true});
    graph.nodes.push_back(LocationNode{LocationId{2}, genesis::world::InvalidLocation, "B", LocationKind::Point, true});
    graph.nodes.push_back(LocationNode{LocationId{3}, genesis::world::InvalidLocation, "C", LocationKind::Point, true});

    graph.edges.push_back(PathEdge{LocationId{2}, LocationId{1}, 1.0f, true});
    graph.edges.push_back(PathEdge{LocationId{2}, LocationId{3}, 2.0f, true});
    graph.edges.push_back(PathEdge{LocationId{3}, LocationId{1}, 1.5f, true});

    return graph;
}

} // namespace

TEST(MovementSystemTest, MovesAgentAlongShortestPath) {
    LocationGraph graph = simpleGraph();
    genesis::world::WorldRegistry world;
    world.setGraph(graph);

    entt::registry registry;
    auto entity = registry.create();

    auto& location = registry.emplace<AgentLocation>(entity);
    location.location = LocationId{2};

    auto& intent = registry.emplace<MovementIntent>(entity);
    intent.target = LocationId{1};
    intent.speed = 1.0f;

    MovementSystem movement(world);

    movement.update(registry, 0.5f);
    EXPECT_EQ(registry.get<AgentLocation>(entity).location.value, 2U);

    movement.update(registry, 0.6f);
    EXPECT_FALSE(registry.any_of<MovementIntent>(entity));
    EXPECT_FALSE(registry.any_of<genesis::agents::components::MovementState>(entity));
    EXPECT_EQ(registry.get<AgentLocation>(entity).location.value, 1U);
}

TEST(MovementSystemTest, RemovesIntentWhenPathUnavailable) {
    LocationGraph graph;
    graph.nodes.push_back(LocationNode{LocationId{1}, genesis::world::InvalidLocation, "Start", LocationKind::Point, true});
    graph.nodes.push_back(LocationNode{LocationId{2}, genesis::world::InvalidLocation, "End", LocationKind::Point, true});
    // No edges between nodes.

    genesis::world::WorldRegistry world;
    world.setGraph(graph);

    entt::registry registry;
    auto entity = registry.create();

    auto& location = registry.emplace<AgentLocation>(entity);
    location.location = LocationId{1};

    auto& intent = registry.emplace<MovementIntent>(entity);
    intent.target = LocationId{2};
    intent.speed = 1.0f;

    MovementSystem movement(world);

    movement.update(registry, 1.0f);

    EXPECT_FALSE(registry.any_of<MovementIntent>(entity));
    EXPECT_FALSE(registry.any_of<genesis::agents::components::MovementState>(entity));
    EXPECT_EQ(registry.get<AgentLocation>(entity).location.value, 1U);
}

