#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "genesis/world/WorldTypes.hpp"

namespace genesis::world::generation {

enum class TerrainType : std::uint8_t {
    Stone,
    Soil
};

struct NoiseGridConfig {
    std::uint32_t width{32};
    std::uint32_t height{32};
    double threshold{0.5};
    double soilSpawnDensity{0.05};
    std::uint32_t resourceCapacity{24};
    std::uint32_t resourceRatePerStep{3};
    bool eightConnectivity{false};
};

struct LayoutNode {
    LocationId id;
    std::string label;
    std::int32_t x{0};
    std::int32_t y{0};
};

struct GenerationLayout {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<LayoutNode> nodes;
};

struct NoiseGenerationResult {
    LocationGraph graph;
    GenerationLayout layout;
};

class NoiseGridGenerator {
public:
    [[nodiscard]] NoiseGenerationResult generate(std::uint64_t seed, const NoiseGridConfig& config) const;
};

void writeNoiseGenerationOutputs(const NoiseGenerationResult& result,
                                 const std::filesystem::path& worldPath,
                                 const std::filesystem::path& layoutPath);

} // namespace genesis::world::generation
