// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/world/map_loading_settings.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace game::world {
namespace {

TEST(MapLoadingSettingsTest, UnknownPreloadModeFallsBackToOff) {
    const std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "tinyfarm_map_loading_settings_test";
    std::filesystem::create_directories(temp_dir);

    const std::filesystem::path config_path = temp_dir / "map_loading_config.json";
    {
        std::ofstream file(config_path);
        ASSERT_TRUE(file.is_open()) << config_path;
        file << R"({
  "preload": {
    "mode": "this_is_not_a_valid_mode"
  }
})";
    }

    const auto settings = MapLoadingSettings::loadFromFile(config_path.string());
    EXPECT_EQ(settings.preload_mode, MapPreloadMode::Off);
}

TEST(MapLoadingSettingsTest, LoadsAsyncPreloadOptionsFromPreloadSection) {
    const std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "tinyfarm_map_loading_settings_test";
    std::filesystem::create_directories(temp_dir);

    const std::filesystem::path config_path = temp_dir / "map_loading_config_async.json";
    {
        std::ofstream file(config_path);
        ASSERT_TRUE(file.is_open()) << config_path;
        file << R"({
  "preload": {
    "mode": "neighbors",
    "async_enabled": true,
    "async_wait_budget_ms": 17,
    "async_submit_wait_ms": 3,
    "async_command_wait_ms": 33,
    "async_worker_count": 2,
    "async_queue_capacity": 128
  },
  "log_timings": true
})";
    }

    const auto settings = MapLoadingSettings::loadFromFile(config_path.string());
    EXPECT_EQ(settings.preload_mode, MapPreloadMode::Neighbors);
    EXPECT_TRUE(settings.async_preload_enabled);
    EXPECT_EQ(settings.async_wait_budget_ms, 17);
    EXPECT_EQ(settings.async_submit_wait_ms, 3);
    EXPECT_EQ(settings.async_command_wait_ms, 33);
    EXPECT_EQ(settings.async_worker_count, 2U);
    EXPECT_EQ(settings.async_queue_capacity, 128U);
    EXPECT_TRUE(settings.log_timings);
}

} // namespace
} // namespace game::world
// NOLINTEND
