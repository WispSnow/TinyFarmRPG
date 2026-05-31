// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/world/map_loading_settings.h"

#include <filesystem>
#include <fstream>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace game::world {
namespace {

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

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

TEST(MapLoadingSettingsTest, LoadsCheckedDefaultsFromRuntimeAssetConfig) {
    const std::filesystem::path config_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/data/map_loading_config.json").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(config_path)) << config_path;

    const auto settings = MapLoadingSettings::loadFromFile(config_path.string());

    EXPECT_EQ(settings.preload_mode, MapPreloadMode::All);
    EXPECT_TRUE(settings.async_preload_enabled);
    EXPECT_EQ(settings.async_wait_budget_ms, 3U);
    EXPECT_EQ(settings.async_submit_wait_ms, 1U);
    EXPECT_EQ(settings.async_command_wait_ms, 8U);
    EXPECT_EQ(settings.async_worker_count, 1U);
    EXPECT_EQ(settings.async_queue_capacity, 32U);
    EXPECT_TRUE(settings.log_timings);
}

TEST(MapLoadingSettingsTest, InvalidJsonKeepsDefaults) {
    const std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "tinyfarm_map_loading_settings_test";
    std::filesystem::create_directories(temp_dir);

    const std::filesystem::path config_path = temp_dir / "map_loading_config_invalid.json";
    {
        std::ofstream file(config_path);
        ASSERT_TRUE(file.is_open()) << config_path;
        file << R"({"preload": )";
    }

    const auto settings = MapLoadingSettings::loadFromFile(config_path.string());
    EXPECT_EQ(settings.preload_mode, MapPreloadMode::All);
    EXPECT_FALSE(settings.log_timings);
    EXPECT_TRUE(settings.async_preload_enabled);
}

TEST(MapLoadingSettingsTest, ClampsOversizedUnsignedValuesBeforeConversion) {
    const std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "tinyfarm_map_loading_settings_test";
    std::filesystem::create_directories(temp_dir);

    const std::filesystem::path config_path = temp_dir / "map_loading_config_large_values.json";
    {
        std::ofstream file(config_path);
        ASSERT_TRUE(file.is_open()) << config_path;
        file << R"({
  "preload": {
    "mode": "all",
    "async_wait_budget_ms": 4294967295,
    "async_submit_wait_ms": 4294967295,
    "async_command_wait_ms": 4294967295,
    "async_worker_count": 999999999999,
    "async_queue_capacity": 999999999999
  }
})";
    }

    const auto settings = MapLoadingSettings::loadFromFile(config_path.string());
    EXPECT_EQ(settings.async_wait_budget_ms, 2000U);
    EXPECT_EQ(settings.async_submit_wait_ms, 2000U);
    EXPECT_EQ(settings.async_command_wait_ms, 2000U);
    EXPECT_EQ(settings.async_worker_count, 64U);
    EXPECT_EQ(settings.async_queue_capacity, 4096U);
}

TEST(MapLoadingSettingsSourceTest, ParserDoesNotUseTryCatchOrJsonValue) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/world/map_loading_settings.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    EXPECT_EQ(content.find("try {"), std::string::npos);
    EXPECT_EQ(content.find("try\n{"), std::string::npos);
    EXPECT_EQ(content.find("catch ("), std::string::npos);
    EXPECT_EQ(content.find("catch("), std::string::npos);
    EXPECT_EQ(content.find(".value(\""), std::string::npos);
    EXPECT_EQ(content.find("->value(\""), std::string::npos);
}

} // namespace
} // namespace game::world
// NOLINTEND
