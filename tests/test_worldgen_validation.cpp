#include <gtest/gtest.h>

#include "genesis/worldgen/ValidationModule.hpp"

using namespace genesis::worldgen;

namespace
{
TopologyDraft make_simple_topology()
{
    TopologyDraft topology{};
    NodeDraft root{};
    root.local_id = 0;
    root.label = "root";
    root.tags = {"hub"};
    topology.nodes.push_back(root);

    NodeDraft child{};
    child.local_id = 1;
    child.parent = 0;
    child.label = "child";
    topology.nodes.push_back(child);

    EdgeDraft edge{};
    edge.from = 0;
    edge.to = 1;
    topology.edges.push_back(edge);

    return topology;
}

LayoutDraft make_layout_with_missing_node()
{
    LayoutDraft layout{};
    layout.placements.push_back(NodePlacement{0, 0.0, 0.0, 0.0});
    return layout;
}

LayoutDraft make_complete_layout()
{
    LayoutDraft layout{};
    layout.placements.push_back(NodePlacement{0, 0.0, 0.0, 0.0});
    layout.placements.push_back(NodePlacement{1, 4.0, 0.0, 0.0});
    return layout;
}
} // namespace

TEST(WorldgenValidation, DetectsDuplicateNodes)
{
    TopologyDraft topology = make_simple_topology();
    topology.nodes.push_back(topology.nodes.back()); // duplicate local_id = 1

    LayoutDraft layout = make_complete_layout();

    ValidationModule validator;
    std::vector<ValidationError> errors;
    const bool ok = validator.validate(topology, layout, errors);
    EXPECT_FALSE(ok);
    ASSERT_FALSE(errors.empty());
    EXPECT_NE(errors.front().message.find("重复"), std::string::npos);
}

TEST(WorldgenValidation, DetectsMissingPlacement)
{
    TopologyDraft topology = make_simple_topology();
    LayoutDraft layout = make_layout_with_missing_node();

    ValidationModule validator;
    std::vector<ValidationError> errors;
    const bool ok = validator.validate(topology, layout, errors);
    EXPECT_FALSE(ok);
    bool found = false;
    for (const auto& err : errors)
    {
        if (err.message.find("布局坐标") != std::string::npos)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(WorldgenValidation, DetectsInvalidEdge)
{
    TopologyDraft topology = make_simple_topology();
    topology.edges.push_back(EdgeDraft{5, 6, true});
    LayoutDraft layout = make_complete_layout();

    ValidationModule validator;
    std::vector<ValidationError> errors;
    const bool ok = validator.validate(topology, layout, errors);
    EXPECT_FALSE(ok);
    bool has_invalid_edge_error = false;
    for (const auto& err : errors)
    {
        if (err.message.find("起点不存在") != std::string::npos || err.message.find("终点不存在") != std::string::npos)
        {
            has_invalid_edge_error = true;
            break;
        }
    }
    EXPECT_TRUE(has_invalid_edge_error);
}

