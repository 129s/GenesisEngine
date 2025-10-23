#include "genesis/rendering/AssetCache.hpp"

#include <stdexcept>
#include <utility>

namespace Genesis::Rendering
{

void ProceduralAssetCache::clear()
{
    m_records.clear();
}

void ProceduralAssetCache::load(const AssetPack& pack)
{
    for (const auto& record : pack.records())
    {
        LookupKey key{record.keyHash, record.asset.lod};
        m_records.insert_or_assign(key, record);
    }
}

void ProceduralAssetCache::loadFromFile(const std::filesystem::path& path)
{
    auto pack = AssetPackReader::loadFromFile(path);
    load(pack);
}

const ProceduralAsset* ProceduralAssetCache::find(std::uint64_t keyHash, LodLevel lod) const noexcept
{
    LookupKey key{keyHash, lod};
    auto it = m_records.find(key);
    if (it == m_records.end())
    {
        return nullptr;
    }
    return &it->second.asset;
}

const ProceduralAsset& ProceduralAssetCache::getOrGenerate(std::uint64_t keyHash,
                                                          LodLevel lod,
                                                          const Generator& generator)
{
    if (const auto* existing = find(keyHash, lod))
    {
        return *existing;
    }

    if (!generator)
    {
        throw std::invalid_argument("generator callback is required when asset missing");
    }

    AssetRecord record = generator(keyHash, lod);
    if (record.keyHash == 0)
    {
        record.keyHash = keyHash;
    }
    record.asset.lod = lod;
    LookupKey key{record.keyHash, record.asset.lod};
    auto [it, inserted] = m_records.insert_or_assign(key, std::move(record));
    (void)inserted;
    return it->second.asset;
}

} // namespace Genesis::Rendering

