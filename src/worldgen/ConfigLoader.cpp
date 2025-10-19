#include "genesis/worldgen/ConfigLoader.hpp"

#include <filesystem>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace genesis::worldgen
{

namespace
{
std::optional<std::size_t> read_size_t(const toml::table& table, std::string_view key)
{
    if (const auto* value = table.get_as<std::int64_t>(key))
    {
        if (*value < 0)
        {
            std::ostringstream oss;
            oss << "`" << key << "` 不能为负数";
            throw std::runtime_error(oss.str());
        }
        return static_cast<std::size_t>(value->get());
    }
    return std::nullopt;
}

ClusterRule parse_cluster_rule(const toml::table& topology_table)
{
    ClusterRule rule{};
    if (const auto* cluster_table = topology_table.get_as<toml::table>("cluster"))
    {
        if (auto min_clusters = read_size_t(*cluster_table, "min_clusters"))
        {
            rule.min_clusters = *min_clusters;
        }
        if (auto max_clusters = read_size_t(*cluster_table, "max_clusters"))
        {
            rule.max_clusters = *max_clusters;
        }
        if (auto min_nodes = read_size_t(*cluster_table, "min_nodes_per_cluster"))
        {
            rule.min_nodes_per_cluster = *min_nodes;
        }
        if (auto max_nodes = read_size_t(*cluster_table, "max_nodes_per_cluster"))
        {
            rule.max_nodes_per_cluster = *max_nodes;
        }
    }
    return rule;
}

CorridorRule parse_corridor_rule(const toml::table& topology_table)
{
    CorridorRule rule{};
    if (const auto* corridor_table = topology_table.get_as<toml::table>("corridor"))
    {
        if (const auto* enabled = corridor_table->get_as<bool>("enabled"))
        {
            rule.enabled = enabled->get();
        }
        if (auto min_length = read_size_t(*corridor_table, "min_length"))
        {
            rule.min_length = *min_length;
        }
        if (auto max_length = read_size_t(*corridor_table, "max_length"))
        {
            rule.max_length = *max_length;
        }
    }
    return rule;
}

TopologySettings parse_topology_settings(const toml::table& root)
{
    TopologySettings settings{};
    if (const auto* topology_table = root.get_as<toml::table>("topology"))
    {
        settings.cluster = parse_cluster_rule(*topology_table);
        settings.corridor = parse_corridor_rule(*topology_table);
    }
    return settings;
}

void validate_config(const toml::table& table, const std::filesystem::path& path)
{
    if (!table.contains("world") || !table["world"].is_table())
    {
        std::ostringstream oss;
        oss << "配置文件缺少 `world` 节点: " << path.string();
        throw std::runtime_error(oss.str());
    }
}
} // namespace

GeneratorConfig load_config(const std::filesystem::path& path)
{
    if (path.empty())
    {
        throw std::runtime_error("配置路径为空");
    }

    const auto resolved = std::filesystem::absolute(path);
    if (!std::filesystem::exists(resolved))
    {
        std::ostringstream oss;
        oss << "配置文件不存在: " << resolved.string();
        throw std::runtime_error(oss.str());
    }

    auto table = toml::parse_file(resolved.string());
    validate_config(table, resolved);

    GeneratorConfig config{};
    config.source_path = resolved;
    config.root = std::move(table);
    config.topology = parse_topology_settings(config.root);
    return config;
}

} // namespace genesis::worldgen
