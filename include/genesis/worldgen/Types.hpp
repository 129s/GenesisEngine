#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <toml++/toml.hpp>

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

struct Seed
{
    std::uint64_t value{0};
};

struct GeneratorConfig
{
    std::filesystem::path source_path{};
    toml::table root{};
    TopologySettings topology{};
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
};

} // namespace genesis::worldgen
