#include "genesis/world/WorldBootstrap.hpp"

namespace genesis::world {

LocationGraph createDemoWorldGraph() {
    LocationGraph graph;

    graph.nodes.push_back(LocationNode{
        LocationId{1}, InvalidLocation, "Town Center", LocationKind::Region, true});
    graph.nodes.push_back(LocationNode{
        LocationId{2}, LocationId{1}, "The Copper Mug Tavern", LocationKind::Building, true});
    graph.nodes.push_back(LocationNode{
        LocationId{3}, LocationId{2}, "Tavern Kitchen", LocationKind::Room, false});
    graph.nodes.push_back(LocationNode{
        LocationId{4}, LocationId{2}, "Tavern Common Hall", LocationKind::Room, true});
    graph.nodes.push_back(LocationNode{
        LocationId{5}, LocationId{1}, "Residential Block A", LocationKind::Building, true});
    graph.nodes.push_back(LocationNode{
        LocationId{6}, LocationId{5}, "Shared Dormitory", LocationKind::Room, true});

    graph.edges.push_back(PathEdge{LocationId{1}, LocationId{2}, 1.0f, true});
    graph.edges.push_back(PathEdge{LocationId{1}, LocationId{5}, 1.2f, true});
    graph.edges.push_back(PathEdge{LocationId{2}, LocationId{4}, 0.5f, true});
    graph.edges.push_back(PathEdge{LocationId{2}, LocationId{3}, 0.3f, false});
    graph.edges.push_back(PathEdge{LocationId{5}, LocationId{6}, 0.4f, true});

    graph.spawns.push_back(ResourceSpawn{
        "Tavern Food Prep", ResourceType::Food, LocationId{3}, 24, 3});
    graph.spawns.push_back(ResourceSpawn{
        "Tavern Hospitality", ResourceType::Social, LocationId{4}, 12, 2});

    return graph;
}

} // namespace genesis::world
