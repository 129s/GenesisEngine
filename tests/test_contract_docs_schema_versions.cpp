#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include "genesis/runtime/SchemaVersions.hpp"
#include "genesis/telemetry/SchemaVersions.hpp"

namespace {

std::filesystem::path repoPath(std::string_view relative) {
    const std::filesystem::path root{GENESIS_TEST_SOURCE_DIR};
    return root / std::filesystem::path(relative);
}

[[nodiscard]] std::string readFileOrEmpty(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

} // namespace

TEST(ContractDocs, TelemetrySchemaDocMatchesCodeVersion) {
    const std::string expected = "TickTelemetry v" + std::to_string(genesis::telemetry::kTickTelemetrySchemaVersion);
    const auto text = readFileOrEmpty(repoPath("docs/architecture/foundation/telemetry-schema.md"));
    ASSERT_FALSE(text.empty()) << "Failed to read docs/architecture/foundation/telemetry-schema.md";
    EXPECT_NE(text.find(expected), std::string::npos) << "Expected to find '" << expected << "'";
}

TEST(ContractDocs, RuntimeApiDocMatchesCodeVersions) {
    const std::string expectedTelemetry = "TickTelemetry 契约（v" + std::to_string(genesis::telemetry::kTickTelemetrySchemaVersion) + "）";
    const std::string expectedAtlas = "WorldAtlas 契约（v" + std::to_string(Genesis::Runtime::kWorldAtlasSchemaVersion) + "）";
    const auto text = readFileOrEmpty(repoPath("docs/architecture/foundation/runtime-api.md"));
    ASSERT_FALSE(text.empty()) << "Failed to read docs/architecture/foundation/runtime-api.md";
    EXPECT_NE(text.find(expectedTelemetry), std::string::npos) << "Expected to find '" << expectedTelemetry << "'";
    EXPECT_NE(text.find(expectedAtlas), std::string::npos) << "Expected to find '" << expectedAtlas << "'";
}

