#include "genesis/worldgen/ConfigLoader.hpp"

#include <filesystem>
#include <cmath>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "genesis/world/WorldTypes.hpp"
#include "genesis/world/ResourceTypeStrings.hpp"

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

std::optional<genesis::world::ResourceType> parse_resource_type(std::string_view sv)
{
    return genesis::world::parseResourceType(sv);
}

std::vector<genesis::world::ResourceType> parse_resource_types(const toml::table& table, std::string_view key)
{
    std::vector<genesis::world::ResourceType> out;
    const auto* node = table.get(key);
    if (!node)
    {
        return out;
    }
    if (!node->is_array())
    {
        std::ostringstream oss;
        oss << "`" << key << "` 必须为数组";
        throw std::runtime_error(oss.str());
    }

    const auto& arr = *node->as_array();
    out.reserve(arr.size());
    for (const auto& item : arr)
    {
        const auto sv = item.value<std::string>();
        if (!sv)
        {
            std::ostringstream oss;
            oss << "`" << key << "` 数组元素必须为字符串";
            throw std::runtime_error(oss.str());
        }
        const auto parsed = parse_resource_type(*sv);
        if (!parsed)
        {
            std::ostringstream oss;
            oss << "未知资源类型: " << *sv;
            throw std::runtime_error(oss.str());
        }
        out.push_back(*parsed);
    }
    return out;
}

std::vector<double> parse_weights(const toml::table& table, std::string_view key)
{
    std::vector<double> out;
    const auto* node = table.get(key);
    if (!node)
    {
        return out;
    }
    if (!node->is_array())
    {
        std::ostringstream oss;
        oss << "`" << key << "` 必须为数组";
        throw std::runtime_error(oss.str());
    }

    const auto& arr = *node->as_array();
    out.reserve(arr.size());
    for (const auto& item : arr)
    {
        if (auto dv = item.value<double>())
        {
            out.push_back(*dv);
            continue;
        }
        if (auto iv = item.value<std::int64_t>())
        {
            out.push_back(static_cast<double>(*iv));
            continue;
        }
        std::ostringstream oss;
        oss << "`" << key << "` 数组元素必须为数字";
        throw std::runtime_error(oss.str());
    }
    return out;
}

std::optional<double> read_double(const toml::table& table, std::string_view key)
{
    if (const auto* value = table.get_as<double>(key))
    {
        return value->get();
    }
    if (const auto* value = table.get_as<std::int64_t>(key))
    {
        return static_cast<double>(value->get());
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

GridLayoutSettings parse_grid_layout(const toml::table& layout_table)
{
    GridLayoutSettings settings{};
    if (const auto* grid_table = layout_table.get_as<toml::table>("grid"))
    {
        if (auto cell_width = read_double(*grid_table, "cell_width"))
        {
            settings.cell_width = *cell_width;
        }
        if (auto cell_height = read_double(*grid_table, "cell_height"))
        {
            settings.cell_height = *cell_height;
        }
        if (auto columns = read_size_t(*grid_table, "columns"))
        {
            settings.columns = *columns;
        }
        if (auto margin = read_double(*grid_table, "margin"))
        {
            settings.margin = *margin;
        }
    }
    return settings;
}

ClusterLayoutSettings parse_cluster_layout(const toml::table& layout_table)
{
    ClusterLayoutSettings settings{};
    if (const auto* cluster_table = layout_table.get_as<toml::table>("cluster"))
    {
        if (auto radial_distance = read_double(*cluster_table, "radial_distance"))
        {
            settings.radial_distance = *radial_distance;
        }
        if (auto radial_step = read_double(*cluster_table, "radial_step"))
        {
            settings.radial_step = *radial_step;
        }
        if (auto node_spacing = read_double(*cluster_table, "node_spacing"))
        {
            settings.node_spacing = *node_spacing;
        }
    }
    return settings;
}

CorridorLayoutSettings parse_corridor_layout(const toml::table& layout_table)
{
    CorridorLayoutSettings settings{};
    if (const auto* corridor_table = layout_table.get_as<toml::table>("corridor"))
    {
        if (auto step = read_double(*corridor_table, "step"))
        {
            settings.step = *step;
        }
    }
    return settings;
}

LayoutSettings parse_layout_settings(const toml::table& root)
{
    LayoutSettings settings{};
    if (const auto* layout_table = root.get_as<toml::table>("layout"))
    {
        settings.grid = parse_grid_layout(*layout_table);
        settings.cluster = parse_cluster_layout(*layout_table);
        settings.corridor = parse_corridor_layout(*layout_table);
        if (const auto* hex_table = layout_table->get_as<toml::table>("hex"))
        {
            if (const auto* enabled = hex_table->get_as<bool>("enabled"))
            {
                settings.hex.enabled = enabled->get();
            }
            if (auto spacing = read_double(*hex_table, "spacing"))
            {
                settings.hex.spacing = *spacing;
            }
        }
        if (const auto* noise_table = layout_table->get_as<toml::table>("noise"))
        {
            if (const auto* enabled = noise_table->get_as<bool>("enabled"))
            {
                settings.noise.enabled = enabled->get();
            }
            if (auto radius = read_double(*noise_table, "radius"))
            {
                settings.noise.radius = *radius;
            }
            if (auto min_spacing = read_double(*noise_table, "min_spacing"))
            {
                settings.noise.min_spacing = *min_spacing;
            }
            if (auto max_attempts = read_size_t(*noise_table, "max_attempts"))
            {
                settings.noise.max_attempts = *max_attempts;
            }
        }
    }
    return settings;
}

TilemapSettings parse_tilemap_settings(const toml::table& root)
{
    TilemapSettings settings{};
    if (const auto* tilemap_table = root.get_as<toml::table>("tilemap"))
    {
        if (auto tile_size = read_size_t(*tilemap_table, "tile_size"))
        {
            settings.tile_size = static_cast<int>(*tile_size);
        }
        if (auto base_extent = read_size_t(*tilemap_table, "base_extent"))
        {
            settings.base_extent = static_cast<int>(*base_extent);
        }
    }
    return settings;
}

WorldDbSettings parse_worlddb_settings(const toml::table& root)
{
    WorldDbSettings settings{};
    if (const auto* db_table = root.get_as<toml::table>("worlddb"))
    {
        if (const auto* res_table = db_table->get_as<toml::table>("resources"))
        {
            if (auto per_map = read_size_t(*res_table, "per_map"))
            {
                settings.resources.per_map = *per_map;
            }

            settings.resources.types = parse_resource_types(*res_table, "types");
            settings.resources.weights = parse_weights(*res_table, "weights");

            if (const auto* type = res_table->get_as<std::string>("type"))
            {
                if (auto parsed = parse_resource_type(type->get()))
                {
                    settings.resources.type = *parsed;
                }
            }
            if (auto capacity = read_size_t(*res_table, "capacity"))
            {
                settings.resources.capacity = static_cast<std::uint32_t>(*capacity);
            }
            if (auto regen = read_size_t(*res_table, "regen_per_step"))
            {
                settings.resources.regen_per_step = static_cast<std::uint32_t>(*regen);
            }

            if (const auto* workshop_node = res_table->get("workshop"))
            {
                if (!workshop_node->is_array())
                {
                    throw std::runtime_error("worlddb.resources.workshop 必须为数组");
                }

                const auto& arr = *workshop_node->as_array();
                settings.resources.workshops.reserve(arr.size());

                const auto parse_inputs = [&](const toml::array& inputs_arr,
                                              std::string_view prefix) -> std::vector<WorldDbResourceSettings::WorkshopInput> {
                    if (inputs_arr.empty())
                    {
                        throw std::runtime_error(std::string(prefix) + "inputs 不能为空");
                    }

                    std::vector<WorldDbResourceSettings::WorkshopInput> inputs;
                    inputs.reserve(inputs_arr.size());

                    for (const auto& input_item : inputs_arr)
                    {
                        if (!input_item.is_table())
                        {
                            throw std::runtime_error(std::string(prefix) + "inputs 数组元素必须为 table");
                        }
                        const auto& in_table = *input_item.as_table();

                        WorldDbResourceSettings::WorkshopInput in{};

                        const auto* in_type = in_table.get_as<std::string>("type");
                        if (!in_type)
                        {
                            throw std::runtime_error(std::string(prefix) + "inputs.type 必须存在且为字符串");
                        }
                        const auto parsedIn = parse_resource_type(in_type->get());
                        if (!parsedIn)
                        {
                            throw std::runtime_error(std::string("未知资源类型: ") + in_type->get());
                        }
                        in.type = *parsedIn;

                        if (auto units = read_size_t(in_table, "units"))
                        {
                            if (*units == 0)
                            {
                                throw std::runtime_error(std::string(prefix) + "inputs.units 必须大于 0");
                            }
                            in.units = static_cast<std::uint32_t>(*units);
                        }
                        else
                        {
                            throw std::runtime_error(std::string(prefix) + "inputs.units 必须存在且为整数");
                        }

                        inputs.push_back(in);
                    }

                    return inputs;
                };

                for (const auto& item : arr)
                {
                    if (!item.is_table())
                    {
                        throw std::runtime_error("worlddb.resources.workshop 数组元素必须为 table");
                    }
                    const auto& w = *item.as_table();

                    WorldDbResourceSettings::Workshop spec{};

                    const auto* output = w.get_as<std::string>("output");
                    if (!output)
                    {
                        throw std::runtime_error("worlddb.resources.workshop.output 必须存在且为字符串");
                    }
                    const auto parsedOutput = parse_resource_type(output->get());
                    if (!parsedOutput)
                    {
                        throw std::runtime_error(std::string("未知资源类型: ") + output->get());
                    }
                    spec.output = *parsedOutput;

                    if (auto initial = read_size_t(w, "initial"))
                    {
                        spec.initial = static_cast<std::uint32_t>(*initial);
                    }

                    if (auto chance = read_double(w, "chance"))
                    {
                        if (!std::isfinite(*chance) || *chance < 0.0 || *chance > 1.0)
                        {
                            throw std::runtime_error("worlddb.resources.workshop.chance 必须在 [0,1] 内");
                        }
                        spec.chance = *chance;
                    }

                    if (const auto* recipes_node = w.get("recipes"))
                    {
                        if (!recipes_node->is_array())
                        {
                            throw std::runtime_error("worlddb.resources.workshop.recipes 必须为数组");
                        }
                        const auto& recipes_arr = *recipes_node->as_array();
                        if (recipes_arr.empty())
                        {
                            throw std::runtime_error("worlddb.resources.workshop.recipes 不能为空");
                        }
                        spec.recipes.reserve(recipes_arr.size());

                        for (const auto& recipe_item : recipes_arr)
                        {
                            if (!recipe_item.is_table())
                            {
                                throw std::runtime_error("worlddb.resources.workshop.recipes 数组元素必须为 table");
                            }
                            const auto& r_table = *recipe_item.as_table();

                            WorldDbResourceSettings::WorkshopRecipe recipe{};
                            if (auto out_units = read_size_t(r_table, "output_units"))
                            {
                                if (*out_units == 0)
                                {
                                    throw std::runtime_error("worlddb.resources.workshop.recipes.output_units 必须大于 0");
                                }
                                recipe.output_units = static_cast<std::uint32_t>(*out_units);
                            }

                            const auto* inputs_node = r_table.get("inputs");
                            if (!inputs_node || !inputs_node->is_array())
                            {
                                throw std::runtime_error("worlddb.resources.workshop.recipes.inputs 必须存在且为数组");
                            }
                            recipe.inputs = parse_inputs(*inputs_node->as_array(), "worlddb.resources.workshop.recipes.");
                            spec.recipes.push_back(std::move(recipe));
                        }
                    }
                    else
                    {
                        WorldDbResourceSettings::WorkshopRecipe recipe{};
                        if (auto out_units = read_size_t(w, "output_units"))
                        {
                            if (*out_units == 0)
                            {
                                throw std::runtime_error("worlddb.resources.workshop.output_units 必须大于 0");
                            }
                            recipe.output_units = static_cast<std::uint32_t>(*out_units);
                        }

                        const auto* inputs_node = w.get("inputs");
                        if (!inputs_node || !inputs_node->is_array())
                        {
                            throw std::runtime_error("worlddb.resources.workshop.inputs 必须存在且为数组");
                        }
                        recipe.inputs = parse_inputs(*inputs_node->as_array(), "worlddb.resources.workshop.");
                        spec.recipes.push_back(std::move(recipe));
                    }

                    settings.resources.workshops.push_back(std::move(spec));
                }
            }

            if (const auto* override_node = res_table->get("map_overrides"))
            {
                if (!override_node->is_array())
                {
                    throw std::runtime_error("worlddb.resources.map_overrides 必须为数组");
                }
                const auto& arr = *override_node->as_array();
                settings.resources.map_overrides.reserve(arr.size());

                for (const auto& item : arr)
                {
                    if (!item.is_table())
                    {
                        throw std::runtime_error("worlddb.resources.map_overrides 数组元素必须为 table");
                    }
                    const auto& t = *item.as_table();

                    WorldDbResourceSettings::MapOverride ov{};
                    if (const auto* map = t.get_as<std::int64_t>("map"))
                    {
                        if (*map <= 0)
                        {
                            throw std::runtime_error("worlddb.resources.map_overrides.map 必须大于 0");
                        }
                        ov.map = static_cast<std::uint32_t>(map->get());
                    }
                    else
                    {
                        throw std::runtime_error("worlddb.resources.map_overrides.map 必须存在且为整数");
                    }

                    ov.types = parse_resource_types(t, "types");
                    ov.weights = parse_weights(t, "weights");

                    const auto& effectiveTypes = ov.types.empty() ? settings.resources.types : ov.types;
                    if (!effectiveTypes.empty())
                    {
                        if (!ov.weights.empty() && ov.weights.size() != effectiveTypes.size())
                        {
                            throw std::runtime_error("worlddb.resources.map_overrides.weights 长度必须与 types 一致");
                        }
                    }
                    else
                    {
                        if (!ov.weights.empty())
                        {
                            throw std::runtime_error("worlddb.resources.map_overrides.weights 仅在提供 types 时可用");
                        }
                    }

                    settings.resources.map_overrides.push_back(std::move(ov));
                }
            }

            if (!settings.resources.types.empty())
            {
                if (!settings.resources.weights.empty() && settings.resources.weights.size() != settings.resources.types.size())
                {
                    throw std::runtime_error("worlddb.resources.weights 长度必须与 types 一致");
                }

                if (settings.resources.weights.empty())
                {
                    settings.resources.weights.assign(settings.resources.types.size(), 1.0);
                }

                double sum = 0.0;
                for (const auto w : settings.resources.weights)
                {
                    if (w < 0.0)
                    {
                        throw std::runtime_error("worlddb.resources.weights 不能为负数");
                    }
                    sum += w;
                }
                if (sum <= 0.0)
                {
                    throw std::runtime_error("worlddb.resources.weights 总和必须大于 0");
                }
            }
        }
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
    config.layout = parse_layout_settings(config.root);
    config.tilemap = parse_tilemap_settings(config.root);
    config.worlddb = parse_worlddb_settings(config.root);
    return config;
}

} // namespace genesis::worldgen
