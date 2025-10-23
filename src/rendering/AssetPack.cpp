#include "genesis/rendering/AssetPack.hpp"

#include <bit>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

#include "genesis/rendering/ProceduralAsset.hpp"

namespace Genesis::Rendering
{
namespace
{

constexpr char kMagic[4] = {'P', 'P', 'A', 'B'};
constexpr std::uint16_t kCurrentVersion = 0x0001;

struct BinaryHeader
{
    char magic[4];
    std::uint16_t version;
    std::uint16_t flags;
    std::uint32_t assetCount;
    std::uint32_t reserved;
    std::uint64_t paletteOffset;
    std::uint64_t tocOffset;
};

struct BinaryAssetEntry
{
    std::uint64_t keyHash;
    std::uint64_t schemaHash;
    std::uint32_t width;
    std::uint32_t height;
    std::uint16_t layerCount;
    std::uint16_t metadataCount;
    std::uint8_t lodLevel;
    std::uint8_t reserved0;
    std::uint16_t reserved1;
    std::uint64_t layerTableOffset;
    std::uint64_t metadataOffset;
};

struct BinaryLayerRecord
{
    std::uint8_t layerType;
    std::uint8_t blendMode;
    std::uint16_t pixelFormat;
    std::int16_t originX;
    std::int16_t originY;
    std::uint16_t reserved;
    std::uint64_t dataOffset;
    std::uint32_t dataLength;
    std::uint32_t width;
    std::uint32_t height;
};

template <typename T>
void writePod(std::ostream& stream, const T& value)
{
    static_assert(std::is_trivially_copyable_v<T>);
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
void readPod(std::istream& stream, T& value)
{
    static_assert(std::is_trivially_copyable_v<T>);
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
}

std::vector<std::uint8_t> encodeMetadata(const std::vector<MetadataEntry>& metadata)
{
    std::vector<std::uint8_t> buffer;
    buffer.reserve(metadata.size() * 8); // heuristic

    for (const auto& entry : metadata)
    {
        if (entry.key.size() > std::numeric_limits<std::uint16_t>::max() ||
            entry.value.size() > std::numeric_limits<std::uint16_t>::max())
        {
            throw std::runtime_error("metadata key/value too large for encoding");
        }
        std::uint16_t keyLen = static_cast<std::uint16_t>(entry.key.size());
        std::uint16_t valueLen = static_cast<std::uint16_t>(entry.value.size());

        auto append16 = [&](std::uint16_t v) {
            buffer.push_back(static_cast<std::uint8_t>(v & 0xFF));
            buffer.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        };

        append16(keyLen);
        append16(valueLen);
        buffer.insert(buffer.end(), entry.key.begin(), entry.key.end());
        buffer.insert(buffer.end(), entry.value.begin(), entry.value.end());
    }

    return buffer;
}

std::vector<MetadataEntry> decodeMetadata(const std::vector<std::uint8_t>& buffer)
{
    std::vector<MetadataEntry> metadata;
    std::size_t offset = 0;
    while (offset + 4 <= buffer.size())
    {
        auto read16 = [&](std::size_t at) -> std::uint16_t {
            return static_cast<std::uint16_t>(buffer.at(at)) |
                   (static_cast<std::uint16_t>(buffer.at(at + 1)) << 8);
        };

        std::uint16_t keyLen = read16(offset);
        std::uint16_t valueLen = read16(offset + 2);
        offset += 4;
        if (offset + keyLen + valueLen > buffer.size())
        {
            throw std::runtime_error("metadata section truncated");
        }

        MetadataEntry entry;
        entry.key.assign(reinterpret_cast<const char*>(&buffer[offset]), keyLen);
        offset += keyLen;
        entry.value.assign(reinterpret_cast<const char*>(&buffer[offset]), valueLen);
        offset += valueLen;
        metadata.push_back(std::move(entry));
    }
    return metadata;
}

void validateLittleEndian()
{
    static_assert(std::endian::native == std::endian::little,
                  "AssetPack only supports little-endian platforms at the moment");
}

} // namespace

void AssetPackWriter::addAsset(AssetRecord record)
{
    m_records.push_back(std::move(record));
}

void AssetPackWriter::clear()
{
    m_records.clear();
}

void AssetPackWriter::writeToFile(const std::filesystem::path& path) const
{
    validateLittleEndian();

    std::ofstream stream(path, std::ios::binary);
    if (!stream)
    {
        throw std::runtime_error("failed to open asset pack for writing: " + path.string());
    }

    BinaryHeader header{};
    std::copy(std::begin(kMagic), std::end(kMagic), header.magic);
    header.version = kCurrentVersion;
    header.flags = 0; // no palette, no compression
    header.assetCount = static_cast<std::uint32_t>(m_records.size());
    header.reserved = 0;
    header.paletteOffset = 0;
    header.tocOffset = sizeof(BinaryHeader);

    const std::size_t tocBytes = sizeof(BinaryAssetEntry) * m_records.size();
    const std::uint64_t bodyOffset = header.tocOffset + tocBytes;

    // Precompute layer/metadata blobs
    struct EncodedAsset
    {
        BinaryAssetEntry entry{};
        std::vector<BinaryLayerRecord> layers;
        std::vector<std::vector<std::uint8_t>> layerPayloads;
        std::vector<std::uint8_t> metadata;
    };

    std::vector<EncodedAsset> encoded;
    encoded.reserve(m_records.size());

    for (const auto& record : m_records)
    {
        EncodedAsset enc{};
        enc.entry.keyHash = record.keyHash;
        enc.entry.schemaHash = record.schemaHash;
        enc.entry.width = static_cast<std::uint32_t>(record.asset.bounds.width);
        enc.entry.height = static_cast<std::uint32_t>(record.asset.bounds.height);
        enc.entry.layerCount = static_cast<std::uint16_t>(record.asset.layers.size());
        enc.entry.metadataCount = static_cast<std::uint16_t>(record.asset.metadata.size());
        enc.entry.lodLevel = static_cast<std::uint8_t>(record.asset.lod);
        enc.entry.reserved0 = 0;
        enc.entry.reserved1 = 0;

        enc.layers.reserve(record.asset.layers.size());
        enc.layerPayloads.reserve(record.asset.layers.size());
        for (const auto& layer : record.asset.layers)
        {
            BinaryLayerRecord binLayer{};
            binLayer.layerType = static_cast<std::uint8_t>(layer.type);
            binLayer.blendMode = static_cast<std::uint8_t>(layer.blend);
            binLayer.pixelFormat = static_cast<std::uint16_t>(layer.format);
            binLayer.originX = static_cast<std::int16_t>(layer.origin.x);
            binLayer.originY = static_cast<std::int16_t>(layer.origin.y);
            binLayer.reserved = 0;
            binLayer.dataOffset = 0; // filled later
            binLayer.dataLength = static_cast<std::uint32_t>(layer.pixels.size());
            binLayer.width = layer.width;
            binLayer.height = layer.height;

            enc.layers.push_back(binLayer);
            enc.layerPayloads.push_back(layer.pixels);
        }

        enc.metadata = encodeMetadata(record.asset.metadata);
        encoded.push_back(std::move(enc));
    }

    // Compute offsets
    std::uint64_t cursor = bodyOffset;
    for (auto& enc : encoded)
    {
        enc.entry.layerTableOffset = cursor;
        cursor += static_cast<std::uint64_t>(enc.layers.size() * sizeof(BinaryLayerRecord));

        for (auto& layer : enc.layers)
        {
            layer.dataOffset = cursor;
            cursor += layer.dataLength;
        }

        enc.entry.metadataOffset = cursor;
        cursor += enc.metadata.size();
    }

    // Write header
    writePod(stream, header);

    // Write TOC placeholder (we will fill with computed entries)
    for (const auto& enc : encoded)
    {
        writePod(stream, enc.entry);
    }

    // Write body (layer tables, payloads, metadata)
    for (const auto& enc : encoded)
    {
        for (const auto& layer : enc.layers)
        {
            writePod(stream, layer);
        }
        for (const auto& payload : enc.layerPayloads)
        {
            if (!payload.empty())
            {
                stream.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
            }
        }
        if (!enc.metadata.empty())
        {
            stream.write(reinterpret_cast<const char*>(enc.metadata.data()),
                         static_cast<std::streamsize>(enc.metadata.size()));
        }
    }

    stream.flush();
    if (!stream)
    {
        throw std::runtime_error("failed to write asset pack: " + path.string());
    }
}

AssetPack::AssetPack(std::vector<AssetRecord> records)
    : m_records(std::move(records))
{
    m_lookup.reserve(m_records.size());
    for (const auto& record : m_records)
    {
        LookupKey key{record.keyHash, record.asset.lod};
        m_lookup[key] = &record;
    }
}

const AssetRecord* AssetPack::find(std::uint64_t keyHash, LodLevel lod) const noexcept
{
    LookupKey key{keyHash, lod};
    auto it = m_lookup.find(key);
    if (it == m_lookup.end())
    {
        return nullptr;
    }
    return it->second;
}

AssetPack AssetPackReader::loadFromFile(const std::filesystem::path& path)
{
    validateLittleEndian();

    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        throw std::runtime_error("failed to open asset pack: " + path.string());
    }

    BinaryHeader header{};
    readPod(stream, header);
    if (std::memcmp(header.magic, kMagic, sizeof(kMagic)) != 0)
    {
        throw std::runtime_error("asset pack magic mismatch");
    }
    if (header.version != kCurrentVersion)
    {
        throw std::runtime_error("asset pack version unsupported");
    }

    // Seek to TOC
    stream.seekg(static_cast<std::streamoff>(header.tocOffset), std::ios::beg);
    std::vector<BinaryAssetEntry> toc(header.assetCount);
    for (auto& entry : toc)
    {
        readPod(stream, entry);
    }

    std::vector<AssetRecord> records;
    records.reserve(toc.size());

    for (std::size_t entryIndex = 0; entryIndex < toc.size(); ++entryIndex)
    {
        const auto& entry = toc[entryIndex];
        AssetRecord record{};
        record.keyHash = entry.keyHash;
        record.schemaHash = entry.schemaHash;

        ProceduralAsset asset{};
        asset.bounds.x = 0;
        asset.bounds.y = 0;
        asset.bounds.width = static_cast<std::int32_t>(entry.width);
        asset.bounds.height = static_cast<std::int32_t>(entry.height);
        asset.lod = static_cast<LodLevel>(entry.lodLevel);

        // Read layer table
        stream.seekg(static_cast<std::streamoff>(entry.layerTableOffset), std::ios::beg);
        std::vector<BinaryLayerRecord> layers(entry.layerCount);
        for (auto& layer : layers)
        {
            readPod(stream, layer);
        }

        asset.layers.reserve(entry.layerCount);
        for (const auto& layer : layers)
        {
            PixelLayer pixelLayer;
            pixelLayer.type = static_cast<LayerType>(layer.layerType);
            pixelLayer.blend = static_cast<BlendMode>(layer.blendMode);
            pixelLayer.format = static_cast<PixelFormat>(layer.pixelFormat);
            pixelLayer.origin.x = layer.originX;
            pixelLayer.origin.y = layer.originY;
            pixelLayer.width = layer.width;
            pixelLayer.height = layer.height;

            if (layer.dataLength > 0)
            {
                pixelLayer.pixels.resize(layer.dataLength);
                stream.seekg(static_cast<std::streamoff>(layer.dataOffset), std::ios::beg);
                stream.read(reinterpret_cast<char*>(pixelLayer.pixels.data()),
                            static_cast<std::streamsize>(layer.dataLength));
                if (!stream)
                {
                    throw std::runtime_error("failed to read layer payload");
                }
            }

            asset.layers.push_back(std::move(pixelLayer));
        }

        // Read metadata
        if (entry.metadataCount > 0)
        {
            std::uint64_t nextOffset = 0;
            if (entryIndex + 1 < toc.size())
            {
                const auto& next = toc[entryIndex + 1];
                nextOffset = next.layerTableOffset;
            }
            else
            {
                stream.seekg(0, std::ios::end);
                nextOffset = static_cast<std::uint64_t>(stream.tellg());
            }

            const std::uint64_t metadataSize = nextOffset > entry.metadataOffset ? (nextOffset - entry.metadataOffset) : 0;
            if (metadataSize > 0)
            {
                std::vector<std::uint8_t> metadataBuf(metadataSize);
                stream.seekg(static_cast<std::streamoff>(entry.metadataOffset), std::ios::beg);
                stream.read(reinterpret_cast<char*>(metadataBuf.data()), static_cast<std::streamsize>(metadataSize));
                if (!stream)
                {
                    throw std::runtime_error("failed to read metadata payload");
                }
                asset.metadata = decodeMetadata(metadataBuf);
            }
        }

        record.asset = std::move(asset);
        records.push_back(std::move(record));
    }

    return AssetPack(std::move(records));
}

} // namespace Genesis::Rendering
