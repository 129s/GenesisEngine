#include "genesis/worldgen/ConfigLoader.hpp"

#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace genesis::worldgen
{

namespace
{
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
    return config;
}

} // namespace genesis::worldgen

