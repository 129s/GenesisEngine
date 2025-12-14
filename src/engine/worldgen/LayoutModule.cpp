#include "genesis/worldgen/LayoutModule.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace genesis::worldgen
{

namespace
{
bool has_tag(const NodeDraft& node, std::string_view tag)
{
    return std::find(node.tags.begin(), node.tags.end(), tag) != node.tags.end();
}

template<typename TValue>
void ensure_positive(TValue value, std::string_view field)
{
    if (value <= static_cast<TValue>(0))
    {
        throw std::invalid_argument(std::string(field) + " 必须为正数");
    }
}

struct ClusterGroup
{
    std::string label;
    std::vector<const NodeDraft*> nodes;
};

std::vector<std::pair<double, double>> generate_hex_offsets(std::size_t count, double spacing)
{
    std::vector<std::pair<double, double>> offsets;
    if (count == 0)
    {
        return offsets;
    }

    offsets.reserve(count);
    offsets.emplace_back(0.0, 0.0);
    std::size_t produced = 1;

    constexpr std::array<std::pair<int, int>, 6> directions = {
        std::pair<int, int>{1, 0},
        {1, -1},
        {0, -1},
        {-1, 0},
        {-1, 1},
        {0, 1}};

    const double sqrt3 = std::sqrt(3.0);

    for (int radius = 1; produced < count; ++radius)
    {
        int q = directions[4].first * radius;
        int r = directions[4].second * radius;

        for (int dir = 0; dir < 6 && produced < count; ++dir)
        {
            for (int step = 0; step < radius && produced < count; ++step)
            {
                const double x = spacing * (sqrt3 * static_cast<double>(q) +
                                            (sqrt3 / 2.0) * static_cast<double>(r));
                const double y = spacing * (1.5 * static_cast<double>(r));
                offsets.emplace_back(x, y);
                ++produced;

                q += directions[dir].first;
                r += directions[dir].second;
            }
        }
    }

    return offsets;
}

std::vector<std::pair<double, double>> generate_grid_offsets(const LayoutSettings& settings, std::size_t count)
{
    std::vector<std::pair<double, double>> offsets;
    offsets.reserve(count);
    const auto columns = static_cast<double>(settings.grid.columns);

    for (std::size_t index = 0; index < count; ++index)
    {
        const double col = static_cast<double>(index % settings.grid.columns);
        const double row = static_cast<double>(index / settings.grid.columns);

        const double offset_x = (col - (columns - 1.0) / 2.0) * settings.grid.cell_width;
        const double offset_y = row * (settings.grid.cell_height + settings.cluster.node_spacing);
        offsets.emplace_back(offset_x, offset_y);
    }
    return offsets;
}

std::vector<std::pair<double, double>> generate_noise_offsets(const NoiseLayoutSettings& settings,
    std::size_t count,
    DeterministicRng& rng)
{
    std::vector<std::pair<double, double>> offsets;
    offsets.reserve(count);

    if (count == 0 || !settings.enabled)
    {
        return offsets;
    }

    constexpr double TWO_PI = 6.28318530717958647692;

    const std::size_t attempts = std::max<std::size_t>(1, settings.max_attempts);

    for (std::size_t i = 0; i < count; ++i)
    {
        bool placed = false;
        for (std::size_t attempt = 0; attempt < attempts; ++attempt)
        {
            const double angle = rng.uniform(0.0, TWO_PI);
            const double distance = rng.uniform(0.0, settings.radius);
            const double x = std::cos(angle) * distance;
            const double y = std::sin(angle) * distance;

            bool valid = true;
            for (const auto& existing : offsets)
            {
                const double dx = existing.first - x;
                const double dy = existing.second - y;
                if (std::sqrt(dx * dx + dy * dy) < settings.min_spacing)
                {
                    valid = false;
                    break;
                }
            }

            if (valid)
            {
                offsets.emplace_back(x, y);
                placed = true;
                break;
            }
        }

        if (!placed)
        {
            offsets.emplace_back(0.0, 0.0);
        }
    }

    return offsets;
}
} // namespace

LayoutModule::LayoutModule(LayoutSettings settings)
    : settings_(std::move(settings))
{
    ensure_positive(settings_.grid.cell_width, "grid.cell_width");
    ensure_positive(settings_.grid.cell_height, "grid.cell_height");
    if (settings_.grid.columns == 0)
    {
        throw std::invalid_argument("grid.columns 必须大于 0");
    }
    ensure_positive(settings_.cluster.radial_distance, "cluster.radial_distance");
    ensure_positive(settings_.cluster.radial_step, "cluster.radial_step");
    ensure_positive(settings_.corridor.step, "corridor.step");
    if (settings_.hex.enabled)
    {
        ensure_positive(settings_.hex.spacing, "hex.spacing");
    }
    if (settings_.noise.enabled)
    {
        ensure_positive(settings_.noise.radius, "noise.radius");
        ensure_positive(settings_.noise.min_spacing, "noise.min_spacing");
        if (settings_.noise.max_attempts == 0)
        {
            throw std::invalid_argument("noise.max_attempts 必须大于 0");
        }
    }
}

LayoutDraft LayoutModule::generate(const TopologyDraft& topology, DeterministicRng& rng) const
{
    LayoutDraft layout{};
    layout.placements.reserve(topology.nodes.size());

    std::unordered_set<std::size_t> placed;
    placed.reserve(topology.nodes.size());

    auto placement_index = [&]() {
        std::unordered_map<std::size_t, NodePlacement*> index;
        index.reserve(layout.placements.size());
        for (auto& p : layout.placements) {
            index.emplace(p.local_id, &p);
        }
        return index;
    };

    auto child_offset = [](DraftNodeKind kind, std::size_t index) -> std::pair<double, double> {
        if (kind == DraftNodeKind::InteractivePortal) {
            constexpr std::array<std::pair<double, double>, 4> offsets = {
                std::pair<double, double>{3.0, -3.0},
                {-3.0, -3.0},
                {3.0, 3.0},
                {-3.0, 3.0},
            };
            return offsets[index % offsets.size()];
        }

        constexpr std::array<std::pair<double, double>, 9> offsets = {
            std::pair<double, double>{2.0, 0.0},
            {-2.0, 0.0},
            {0.0, 2.0},
            {0.0, -2.0},
            {2.0, 2.0},
            {-2.0, 2.0},
            {2.0, -2.0},
            {-2.0, -2.0},
            {0.0, 0.0},
        };
        return offsets[index % offsets.size()];
    };

    // 1. Root/hub
    for (const auto& node : topology.nodes)
    {
        if (has_tag(node, "hub") || node.label == "root")
        {
            layout.placements.push_back(NodePlacement{node.local_id, 0.0, 0.0, 0.0});
            placed.insert(node.local_id);
            break;
        }
    }

    // 2. Cluster grouping
    std::vector<ClusterGroup> cluster_groups;
    cluster_groups.reserve(topology.nodes.size());
    for (const auto& node : topology.nodes)
    {
        if (placed.contains(node.local_id))
        {
            continue;
        }

        if (has_tag(node, "cluster"))
        {
            auto it = std::find_if(cluster_groups.begin(), cluster_groups.end(), [&](const ClusterGroup& group) {
                return group.label == node.label;
            });
            if (it == cluster_groups.end())
            {
                cluster_groups.push_back(ClusterGroup{node.label, {&node}});
            }
            else
            {
                it->nodes.push_back(&node);
            }
        }
    }

    const std::size_t cluster_count = cluster_groups.size();

    constexpr double TWO_PI = 6.28318530717958647692;

    for (std::size_t index = 0; index < cluster_groups.size(); ++index)
    {
        auto& group = cluster_groups[index];
        std::sort(group.nodes.begin(), group.nodes.end(), [](const NodeDraft* a, const NodeDraft* b) {
            return a->local_id < b->local_id;
        });

        const double angle = cluster_count > 0 ? (TWO_PI * static_cast<double>(index) / static_cast<double>(cluster_count))
                                               : 0.0;
        const double radius = settings_.cluster.radial_distance + settings_.cluster.radial_step * static_cast<double>(index);

        const double anchor_x = std::cos(angle) * radius;
        const double anchor_y = std::sin(angle) * radius;

        std::vector<std::pair<double, double>> offsets = settings_.hex.enabled
            ? generate_hex_offsets(group.nodes.size(), settings_.hex.spacing)
            : generate_grid_offsets(settings_, group.nodes.size());

        for (std::size_t node_index = 0; node_index < group.nodes.size(); ++node_index)
        {
            const auto* node = group.nodes[node_index];
            const auto offset = offsets[node_index];

            layout.placements.push_back(NodePlacement{
                node->local_id,
                anchor_x + offset.first,
                anchor_y + offset.second,
                0.0});
            placed.insert(node->local_id);
        }
    }

    // 3. Corridor nodes (linear chain)
    std::vector<const NodeDraft*> corridor_nodes;
    for (const auto& node : topology.nodes)
    {
        if (!placed.contains(node.local_id) && has_tag(node, "corridor"))
        {
            corridor_nodes.push_back(&node);
        }
    }
    std::sort(corridor_nodes.begin(), corridor_nodes.end(), [](const NodeDraft* a, const NodeDraft* b) {
        return a->local_id < b->local_id;
    });

    double corridor_offset = settings_.cluster.radial_distance +
        settings_.cluster.radial_step * static_cast<double>(cluster_count + 1);

    for (std::size_t i = 0; i < corridor_nodes.size(); ++i)
    {
        const auto* node = corridor_nodes[i];
        const double x = corridor_offset + settings_.corridor.step * static_cast<double>(i + 1);
        layout.placements.push_back(NodePlacement{node->local_id, x, 0.0, 0.0});
        placed.insert(node->local_id);
    }

    // 4. Noise nodes
    std::vector<const NodeDraft*> noise_nodes;
    for (const auto& node : topology.nodes)
    {
        if (!placed.contains(node.local_id) && (has_tag(node, "wilds") || has_tag(node, "noise")))
        {
            noise_nodes.push_back(&node);
        }
    }
    if (!noise_nodes.empty() && settings_.noise.enabled)
    {
        auto noise_offsets = generate_noise_offsets(settings_.noise, noise_nodes.size(), rng);
        for (std::size_t i = 0; i < noise_nodes.size(); ++i)
        {
            const auto* node = noise_nodes[i];
            const auto offset = noise_offsets[i];
            layout.placements.push_back(NodePlacement{node->local_id, offset.first, offset.second, 0.0});
            placed.insert(node->local_id);
        }
    }

    // 4b. Child nodes (place near parent)
    {
        auto index = placement_index();
        std::unordered_map<std::size_t, std::size_t> child_count;
        child_count.reserve(topology.nodes.size());

        for (const auto& node : topology.nodes)
        {
            if (placed.contains(node.local_id) || !node.parent)
            {
                continue;
            }

            const auto parent_id = *node.parent;
            auto it = index.find(parent_id);
            if (it == index.end())
            {
                continue;
            }

            const std::size_t order = child_count[parent_id]++;
            const auto offset = child_offset(node.kind, order);
            layout.placements.push_back(NodePlacement{
                node.local_id,
                it->second->x + offset.first,
                it->second->y + offset.second,
                0.0});
            placed.insert(node.local_id);
            index.emplace(node.local_id, &layout.placements.back());
        }
    }

    // 5. Fallback for nodes without placement
    for (const auto& node : topology.nodes)
    {
        if (!placed.contains(node.local_id))
        {
            layout.placements.push_back(NodePlacement{node.local_id, 0.0, 0.0, 0.0});
            placed.insert(node.local_id);
        }
    }

    std::sort(layout.placements.begin(),
        layout.placements.end(),
        [](const NodePlacement& a, const NodePlacement& b) { return a.local_id < b.local_id; });

    return layout;
}

} // namespace genesis::worldgen
