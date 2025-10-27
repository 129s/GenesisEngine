#include "genesis/rendering/ProceduralAsset.hpp"

#include <algorithm>

namespace Genesis::Rendering
{

std::size_t bytesPerPixel(PixelFormat format) noexcept
{
    switch (format)
    {
    case PixelFormat::Index4:
        return 0; // packed format (two pixels per byte)
    case PixelFormat::Index8:
        return 1;
    case PixelFormat::RGBA8888:
        return 4;
    case PixelFormat::Mask1Bit:
        return 0; // packed format (eight pixels per byte)
    }
    return 0;
}

bool isIndexedFormat(PixelFormat format) noexcept
{
    return format == PixelFormat::Index4 || format == PixelFormat::Index8;
}

std::uint16_t maxPaletteEntries(PixelFormat format) noexcept
{
    switch (format)
    {
    case PixelFormat::Index4:
        return 16;
    case PixelFormat::Index8:
        return 256;
    default:
        return 0;
    }
}

const MetadataEntry* findMetadata(const ProceduralAsset& asset, std::string_view key) noexcept
{
    auto it = std::find_if(asset.metadata.begin(), asset.metadata.end(), [&](const MetadataEntry& entry) {
        return entry.key == key;
    });
    if (it == asset.metadata.end())
    {
        return nullptr;
    }
    return &*it;
}

} // namespace Genesis::Rendering
