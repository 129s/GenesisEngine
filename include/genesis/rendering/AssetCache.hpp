#pragma once

#include "genesis/rendering/AssetPack.hpp"

#include <functional>
#include <unordered_map>

namespace Genesis::Rendering
{

class ProceduralAssetCache
{
public:
    using Generator = std::function<AssetRecord(std::uint64_t keyHash, LodLevel lod)>;

    void clear();

    void load(const AssetPack& pack);
    void loadFromFile(const std::filesystem::path& path);

    [[nodiscard]] const ProceduralAsset* find(std::uint64_t keyHash, LodLevel lod) const noexcept;
    [[nodiscard]] const ProceduralAsset& getOrGenerate(std::uint64_t keyHash, LodLevel lod, const Generator& generator);

    [[nodiscard]] std::size_t size() const noexcept { return m_records.size(); }

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

    std::unordered_map<LookupKey, AssetRecord, KeyHasher> m_records;
};

} // namespace Genesis::Rendering

