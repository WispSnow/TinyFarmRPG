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

} // namespace
} // namespace game::world
// NOLINTEND

