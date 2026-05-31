// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/data/game_time.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace game::data {
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

TEST(GameTimeConfigTest, InvalidConfigFileReturnsDefaultInstance) {
    const auto path = tempPath("tinyfarm_game_time_invalid_config.json");
    ASSERT_TRUE(writeTextFile(path, R"({"minutes_per_real_second": )"));

    const auto game_time = GameTime::loadFromConfig(path.string());
    ASSERT_NE(game_time, nullptr);
    EXPECT_FLOAT_EQ(game_time->config_.minutes_per_real_second_, 1.0F);
    EXPECT_EQ(game_time->day_, 1U);
    EXPECT_FLOAT_EQ(game_time->hour_, 6.0F);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST(GameTimeConfigTest, TypedReadIgnoresMismatchedFieldsWithoutClobberingExistingValues) {
    GameTime game_time{};
    game_time.config_.minutes_per_real_second_ = 7.0F;
    game_time.config_.dawn_start_hour_ = 3.0F;
    game_time.config_.dawn_end_hour_ = 8.0F;
    game_time.day_ = 9U;
    game_time.hour_ = 10.0F;
    game_time.minute_ = 15.0F;

    const nlohmann::json config = {
        {"minutes_per_real_second", "fast"},
        {"time_periods", {
            {"dawn", {
                {"start", "early"},
                {"end", 9.0},
            }},
        }},
        {"initial_time", {
            {"day", "tomorrow"},
            {"hour", true},
            {"minute", 30.0},
        }},
    };

    EXPECT_TRUE(game_time.loadConfigFromJson(config));
    EXPECT_FLOAT_EQ(game_time.config_.minutes_per_real_second_, 7.0F);
    EXPECT_FLOAT_EQ(game_time.config_.dawn_start_hour_, 3.0F);
    EXPECT_FLOAT_EQ(game_time.config_.dawn_end_hour_, 9.0F);
    EXPECT_EQ(game_time.day_, 9U);
    EXPECT_FLOAT_EQ(game_time.hour_, 10.0F);
    EXPECT_FLOAT_EQ(game_time.minute_, 30.0F);
}

TEST(GameTimeConfigTest, InvalidEmissiveVisibilityConfigKeepsExistingWindow) {
    const auto path = tempPath("tinyfarm_emissive_visibility_invalid_config.json");
    ASSERT_TRUE(writeTextFile(path, R"({"emissive_visibility": )"));

    GameTime game_time{};
    game_time.emissive_dark_start_hour_ = 17.0F;
    game_time.emissive_dark_end_hour_ = 5.0F;

    EXPECT_FALSE(game_time.loadEmissiveVisibilityFromLightConfig(path.string()));
    EXPECT_FLOAT_EQ(game_time.emissive_dark_start_hour_, 17.0F);
    EXPECT_FLOAT_EQ(game_time.emissive_dark_end_hour_, 5.0F);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

} // namespace game::data
// NOLINTEND
