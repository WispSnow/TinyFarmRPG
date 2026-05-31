// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/system/day_night_system.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace game::system {
namespace {

[[nodiscard]] std::filesystem::path tempPath(std::string_view name) {
    return std::filesystem::temp_directory_path() / std::string(name);
}

[[nodiscard]] bool writeTextFile(const std::filesystem::path& path, std::string_view content) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << content;
    return file.good();
}

} // namespace

TEST(DayNightSystemConfigTest, InvalidJsonDoesNotClobberExistingLightingConfig) {
    const auto path = tempPath("tinyfarm_light_config_invalid.json");
    ASSERT_TRUE(writeTextFile(path, R"({"sun": )"));

    LightingConfig config{};
    config.sun.softness = 0.77F;
    config.moon.intensity = 0.42F;

    EXPECT_FALSE(config.loadFromFile(path.string()));
    EXPECT_FLOAT_EQ(config.sun.softness, 0.77F);
    EXPECT_FLOAT_EQ(config.moon.intensity, 0.42F);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST(DayNightSystemConfigTest, TypedReadKeepsFallbacksForMismatchedFields) {
    const auto path = tempPath("tinyfarm_light_config_mixed_types.json");
    ASSERT_TRUE(writeTextFile(path, R"json({
  "sun": {
    "color": { "warmth_variation": 0.35 },
    "intensity": { "base_min": "dim", "base_max": 0.9 },
    "softness": "soft"
  },
  "moon": {
    "color": { "r": 0.2, "g": "blue", "b": 0.7 },
    "intensity": 0.33
  },
  "ambient": {
    "keyframes": [
      { "hour": 6.0, "color": { "r": 0.1, "g": 0.2, "b": 0.3 } },
      { "hour": "noon", "color": { "r": "bad", "g": 0.4, "b": 0.5 } }
    ]
  }
})json"));

    LightingConfig config{};
    config.sun.intensity_base_min = 0.25F;
    config.sun.softness = 0.44F;
    config.moon.color.g = 0.8F;

    EXPECT_TRUE(config.loadFromFile(path.string()));
    EXPECT_FLOAT_EQ(config.sun.warmth_variation, 0.35F);
    EXPECT_FLOAT_EQ(config.sun.intensity_base_min, 0.25F);
    EXPECT_FLOAT_EQ(config.sun.intensity_base_max, 0.9F);
    EXPECT_FLOAT_EQ(config.sun.softness, 0.44F);
    EXPECT_FLOAT_EQ(config.moon.color.r, 0.2F);
    EXPECT_FLOAT_EQ(config.moon.color.g, 0.8F);
    EXPECT_FLOAT_EQ(config.moon.color.b, 0.7F);
    EXPECT_FLOAT_EQ(config.moon.intensity, 0.33F);
    ASSERT_EQ(config.ambient_keyframes.size(), 2U);
    EXPECT_FLOAT_EQ(config.ambient_keyframes[0].hour, 6.0F);
    EXPECT_FLOAT_EQ(config.ambient_keyframes[0].color.r, 0.1F);
    EXPECT_FLOAT_EQ(config.ambient_keyframes[1].hour, 0.0F);
    EXPECT_FLOAT_EQ(config.ambient_keyframes[1].color.g, 0.4F);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

} // namespace game::system
// NOLINTEND
