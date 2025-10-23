#include <gtest/gtest.h>

#include "genesis/style/ColorRegistry.hpp"

#include <cmath>
#include <filesystem>

namespace
{

std::filesystem::path palettePath()
{
    auto path = std::filesystem::path(__FILE__);
    auto root = path.parent_path().parent_path();
    return root / "data" / "palette" / "core.json";
}

float srgbChannelToLinear(float value)
{
    if (value <= 0.04045f)
    {
        return value / 12.92f;
    }
    return std::pow((value + 0.055f) / 1.055f, 2.4f);
}

} // namespace

TEST(ColorRegistry, LoadsCorePalette)
{
    auto& registry = Genesis::Style::ColorRegistry::instance();
    registry.unload();

    const auto path = palettePath();
    ASSERT_TRUE(std::filesystem::exists(path));

    ASSERT_TRUE(registry.tryLoadFrom(path));
    EXPECT_EQ(registry.activeTheme(), "genesis/core-dark");

    const auto primary = registry.color("accent.primary.base");
    ASSERT_TRUE(primary.has_value());
    EXPECT_NEAR(primary->r, 0.196f, 1e-3f);
    EXPECT_NEAR(primary->g, 0.765f, 1e-3f);
    EXPECT_NEAR(primary->b, 1.0f, 1e-3f);
    EXPECT_NEAR(primary->a, 1.0f, 1e-6f);

    const auto alias = registry.color("Primary");
    ASSERT_TRUE(alias.has_value());
    EXPECT_NEAR(alias->g, primary->g, 1e-6f);
}

TEST(ColorRegistry, ProducesLinearSpace)
{
    auto& registry = Genesis::Style::ColorRegistry::instance();
    if (!registry.isLoaded())
    {
        ASSERT_TRUE(registry.tryLoadFrom(palettePath()));
    }

    const auto srgb = registry.color("surface.canvas", Genesis::Style::ColorSpace::SRGB);
    const auto linear = registry.color("surface.canvas", Genesis::Style::ColorSpace::Linear);
    ASSERT_TRUE(srgb.has_value());
    ASSERT_TRUE(linear.has_value());

    EXPECT_NEAR(linear->r, srgbChannelToLinear(srgb->r), 1e-4f);
    EXPECT_NEAR(linear->g, srgbChannelToLinear(srgb->g), 1e-4f);
    EXPECT_NEAR(linear->b, srgbChannelToLinear(srgb->b), 1e-4f);
    EXPECT_NEAR(linear->a, srgb->a, 1e-6f);
}

