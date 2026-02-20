#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "engine/core/config.h"

namespace engine::core {
namespace {

[[nodiscard]] std::filesystem::path makeTempConfigPath() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("tinyfarm_loop_timing_config_" + std::to_string(now) + ".json");
}

[[nodiscard]] bool writeTextFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path);
    if (!out.is_open()) {
        return false;
    }
    out << content;
    return out.good();
}

} // namespace

TEST(ConfigLoopTimingTest, LoadsPerformanceLoopSettings) {
    const auto path = makeTempConfigPath();

    ASSERT_TRUE(writeTextFile(path, R"json(
{
  "performance": {
    "target_fps": 120,
    "logic_tick_hz": 50,
    "max_ticks_per_frame": 7,
    "render_interpolation": true
  }
}
)json"));

    const auto cleanup = [&]() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    };

    auto config = Config::create(path.string());
    ASSERT_NE(config, nullptr);

    EXPECT_EQ(config->target_fps_, 120);
    EXPECT_EQ(config->logic_tick_hz_, 50);
    EXPECT_EQ(config->max_ticks_per_frame_, 7);
    EXPECT_TRUE(config->render_interpolation_enabled_);

    cleanup();
}

TEST(ConfigLoopTimingTest, InvalidLoopSettingsAreClamped) {
    const auto path = makeTempConfigPath();

    ASSERT_TRUE(writeTextFile(path, R"json(
{
  "performance": {
    "target_fps": -1,
    "logic_tick_hz": 0,
    "max_ticks_per_frame": 0,
    "render_interpolation": false
  }
}
)json"));

    const auto cleanup = [&]() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    };

    auto config = Config::create(path.string());
    ASSERT_NE(config, nullptr);

    EXPECT_EQ(config->target_fps_, 0);
    EXPECT_EQ(config->logic_tick_hz_, 60);
    EXPECT_EQ(config->max_ticks_per_frame_, 1);
    EXPECT_FALSE(config->render_interpolation_enabled_);

    cleanup();
}

} // namespace engine::core
