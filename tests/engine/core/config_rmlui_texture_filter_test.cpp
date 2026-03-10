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
    return std::filesystem::temp_directory_path() / ("tinyfarm_rmlui_texture_filter_config_" + std::to_string(now) + ".json");
}

[[nodiscard]] bool writeTextFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path);
    if (!out.is_open()) {
        return false;
    }
    out << content;
    return out.good();
}

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

} // namespace

TEST(ConfigRmlUiTextureFilterTest, LoadsNearestAndLinearModes) {
    const auto path = makeTempConfigPath();

    ASSERT_TRUE(writeTextFile(path, R"json(
{
  "graphics": {
    "rmlui_texture_filter": "linear"
  }
}
)json"));

    auto config = Config::create(path.string());
    ASSERT_NE(config, nullptr);
    EXPECT_EQ(config->rmlui_texture_filter_mode_, engine::ui::rmlui::RmlUiTextureFilterMode::Linear);

    ASSERT_TRUE(writeTextFile(path, R"json(
{
  "graphics": {
    "rmlui_texture_filter": "nearest"
  }
}
)json"));

    config = Config::create(path.string());
    ASSERT_NE(config, nullptr);
    EXPECT_EQ(config->rmlui_texture_filter_mode_, engine::ui::rmlui::RmlUiTextureFilterMode::Nearest);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST(ConfigRmlUiTextureFilterTest, InvalidModeFallsBackToNearest) {
    const auto path = makeTempConfigPath();

    ASSERT_TRUE(writeTextFile(path, R"json(
{
  "graphics": {
    "rmlui_texture_filter": "cubic"
  }
}
)json"));

    auto config = Config::create(path.string());
    ASSERT_NE(config, nullptr);
    EXPECT_EQ(config->rmlui_texture_filter_mode_, engine::ui::rmlui::RmlUiTextureFilterMode::Nearest);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST(ConfigRmlUiTextureFilterTest, SavePersistsTextureFilterMode) {
    const auto path = makeTempConfigPath();

    auto config = Config::create(path.string());
    ASSERT_NE(config, nullptr);

    config->rmlui_texture_filter_mode_ = engine::ui::rmlui::RmlUiTextureFilterMode::Linear;
    ASSERT_TRUE(config->saveToFile(path.string()));

    const std::string saved = readTextFile(path);
    ASSERT_FALSE(saved.empty());
    EXPECT_NE(saved.find("\"rmlui_texture_filter\": \"linear\""), std::string::npos);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

} // namespace engine::core
