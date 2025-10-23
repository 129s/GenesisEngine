#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Genesis::Rendering
{

enum class PixelFormat : std::uint8_t
{
    Index4 = 0,
    Index8 = 1,
    RGBA8888 = 2,
    Mask1Bit = 3,
};

enum class BlendMode : std::uint8_t
{
    Opaque = 0,
    AlphaStep = 1,
    DitherMask = 2,
    Add = 3,
    Multiply = 4,
};

enum class LayerType : std::uint8_t
{
    Base = 0,
    Seam = 1,
    Lighting = 2,
    Fitting = 3,
    Wear = 4,
    Decal = 5,
    Signage = 6,
    Custom = 255,
};

enum class LodLevel : std::uint8_t
{
    LOD0 = 0,
    LOD1 = 1,
    LOD2 = 2,
};

struct Int2
{
    std::int32_t x{0};
    std::int32_t y{0};
};

struct RectI
{
    std::int32_t x{0};
    std::int32_t y{0};
    std::int32_t width{0};
    std::int32_t height{0};
};

struct PixelLayer
{
    LayerType type{LayerType::Base};
    BlendMode blend{BlendMode::Opaque};
    PixelFormat format{PixelFormat::RGBA8888};
    Int2 origin{};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::uint8_t> pixels; // tightly packed row-major data

    [[nodiscard]] std::size_t pixelCount() const noexcept
    {
        return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    }
};

struct MetadataEntry
{
    std::string key;
    std::string value;
};

struct ProceduralAsset
{
    RectI bounds{};
    LodLevel lod{LodLevel::LOD1};
    std::vector<PixelLayer> layers;
    std::vector<MetadataEntry> metadata;
};

[[nodiscard]] std::size_t bytesPerPixel(PixelFormat format) noexcept;
[[nodiscard]] bool isIndexedFormat(PixelFormat format) noexcept;
[[nodiscard]] std::uint16_t maxPaletteEntries(PixelFormat format) noexcept;

[[nodiscard]] const MetadataEntry* findMetadata(const ProceduralAsset& asset, std::string_view key) noexcept;

} // namespace Genesis::Rendering
