#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <toml++/toml.hpp>

namespace genesis::worldgen
{

struct Seed
{
    std::uint64_t value{0};
};

struct GeneratorConfig
{
    std::filesystem::path source_path{};
    toml::table root{};
};

struct GenerationLogEntry
{
    std::string message;
};

struct GeneratedWorld
{
    Seed seed{};
    std::size_t location_count{0};
    std::size_t edge_count{0};
    std::vector<GenerationLogEntry> logs{};
};

} // namespace genesis::worldgen

