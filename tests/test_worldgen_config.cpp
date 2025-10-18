#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "genesis/worldgen/ConfigLoader.hpp"

namespace fs = std::filesystem;
using namespace genesis::worldgen;

namespace
{
struct TempTomlFile
{
    fs::path path;

    explicit TempTomlFile(std::string_view content)
    {
        auto tmp_dir = fs::temp_directory_path();
        path = tmp_dir / fs::path("genesis_worldgen_config_test.toml");

        std::ofstream out(path, std::ios::trunc);
        out << content;
        out.flush();
    }

    ~TempTomlFile()
    {
        std::error_code ec;
        fs::remove(path, ec);
    }
};
} // namespace

TEST(WorldgenConfig, LoadConfigSuccess)
{
    TempTomlFile file(R"(
        [world]
        name = "demo"
    )");

    auto config = load_config(file.path);
    EXPECT_EQ(config.source_path, fs::absolute(file.path));
    ASSERT_TRUE(config.root.contains("world"));
    ASSERT_TRUE(config.root["world"].is_table());
    const auto name = config.root["world"]["name"].value<std::string>().value_or("");
    EXPECT_EQ(name, "demo");
}

TEST(WorldgenConfig, MissingWorldSectionThrows)
{
    TempTomlFile file(R"(
        [topology]
        clusters = 2
    )");

    EXPECT_THROW({ load_config(file.path); }, std::runtime_error);
}
