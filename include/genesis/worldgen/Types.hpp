#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <toml++/toml.hpp>

#include "genesis/world/WorldTypes.hpp"

namespace genesis::worldgen
{

enum class DraftNodeKind : std::uint8_t
{
    Scene,
    InteractivePortal,
    InteractiveResource
};

struct NodeDraft
{
    std::size_t local_id{0};
    DraftNodeKind kind{DraftNodeKind::Scene};
    std::string label;
    std::optional<std::size_t> parent;
    std::vector<std::string> tags;
};

struct EdgeDraft
{
    std::size_t from{0};
    std::size_t to{0};
    bool bidirectional{true};
};

struct TopologyDraft
{
    std::vector<NodeDraft> nodes;
    std::vector<EdgeDraft> edges;
    struct Portal
    {
        std::size_t entry{0};
        std::size_t exit{0};
    };
    std::vector<Portal> portals;
};

struct ClusterRule
{
    std::size_t min_clusters{1};
    std::size_t max_clusters{1};
    std::size_t min_nodes_per_cluster{2};
    std::size_t max_nodes_per_cluster{4};
};

struct CorridorRule
{
    bool enabled{false};
    std::size_t min_length{1};
    std::size_t max_length{3};
};

struct TopologySettings
{
    ClusterRule cluster{};
    CorridorRule corridor{};
};

struct NodePlacement
{
    std::size_t local_id{0};
    double x{0.0};
    double y{0.0};
    double rotation{0.0};
};

struct LayoutDraft
{
    std::vector<NodePlacement> placements;
};

struct GridLayoutSettings
{
    double cell_width{4.0};
    double cell_height{4.0};
    std::size_t columns{3};
    double margin{2.0};
};

struct ClusterLayoutSettings
{
    double radial_distance{18.0};
    double radial_step{6.0};
    double node_spacing{2.0};
};

struct CorridorLayoutSettings
{
    double step{8.0};
};

struct HexLayoutSettings
{
    bool enabled{false};
    double spacing{6.0};
};

struct NoiseLayoutSettings
{
    bool enabled{false};
    double radius{20.0};
    double min_spacing{3.0};
    std::size_t max_attempts{64};
};

struct LayoutSettings
{
    GridLayoutSettings grid{};
    ClusterLayoutSettings cluster{};
    CorridorLayoutSettings corridor{};
    HexLayoutSettings hex{};
    NoiseLayoutSettings noise{};
};

struct TilemapSettings
{
    int tile_size{32};
    int base_extent{32};
};

struct Seed
{
    std::uint64_t value{0};
};

struct GeneratorConfig
{
    std::filesystem::path source_path{};
    toml::table root{};
    TopologySettings topology{};
    LayoutSettings layout{};
    TilemapSettings tilemap{};
};

struct GenerationLogEntry
{
    std::string message;
};

struct GeneratedWorld
{
    Seed seed{};
    std::size_t location_count{0};
    std::size_t edge_count{0};
    std::vector<GenerationLogEntry> logs{};
    TopologyDraft topology{};
    LayoutDraft layout{};
    world::LocationGraph world_graph{};
};

} // namespace genesis::worldgen
