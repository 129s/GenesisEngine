#pragma once

#include "genesis/rendering/ProceduralAsset.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace Genesis::Rendering
{

struct AssetRecord
{
    std::uint64_t keyHash{0};
    std::uint64_t schemaHash{0};
    ProceduralAsset asset;
};

class AssetPackWriter
{
public:
    void addAsset(AssetRecord record);
    void clear();

    void writeToFile(const std::filesystem::path& path) const;

private:
    std::vector<AssetRecord> m_records;
};

class AssetPack
{
public:
    AssetPack() = default;
    explicit AssetPack(std::vector<AssetRecord> records);

    [[nodiscard]] const AssetRecord* find(std::uint64_t keyHash, LodLevel lod) const noexcept;
    [[nodiscard]] std::span<const AssetRecord> records() const noexcept { return m_records; }

private:
    struct LookupKey
    {
        std::uint64_t keyHash;
        LodLevel lod;

        bool operator==(const LookupKey& other) const noexcept
        {
            return keyHash == other.keyHash && lod == other.lod;
        }
    };

    struct KeyHasher
    {
        std::size_t operator()(const LookupKey& key) const noexcept
        {
            return std::hash<std::uint64_t>{}(key.keyHash) ^ (static_cast<std::size_t>(key.lod) << 1);
        }
    };

    std::vector<AssetRecord> m_records;
    std::unordered_map<LookupKey, const AssetRecord*, KeyHasher> m_lookup;
};

class AssetPackReader
{
public:
    AssetPackReader() = default;

    static AssetPack loadFromFile(const std::filesystem::path& path);
};

} // namespace Genesis::Rendering

