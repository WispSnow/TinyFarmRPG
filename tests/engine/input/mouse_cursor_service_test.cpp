#include "engine/input/mouse_cursor_service.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace {

[[nodiscard]] nlohmann::json parseJsonFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return nlohmann::json::parse(content, nullptr, false);
}

} // namespace

TEST(MouseCursorServiceTest, ParsesProjectCursorConfig) {
    const auto root = parseJsonFile(std::filesystem::path(PROJECT_SOURCE_DIR) / "assets/data/cursor_config.json");
    ASSERT_FALSE(root.is_discarded());

    engine::input::MouseCursorTheme theme{};
    std::string error;
    ASSERT_TRUE(engine::input::parseMouseCursorThemeJson(root, theme, &error)) << error;

    ASSERT_TRUE(root.contains("hotspot_policy"));
    EXPECT_EQ(root["hotspot_policy"].get<std::string>(), "stable_origin");
    EXPECT_EQ(theme.sheet, "assets/farm-rpg/UI/HUD.png");
    EXPECT_EQ(theme.theme_id, "warm_light");
    EXPECT_EQ(theme.tile_size, 16);
    EXPECT_EQ(theme.scale, 2);
    EXPECT_TRUE(theme.states[static_cast<std::size_t>(engine::input::MouseCursorKind::Default)].has_value());
    EXPECT_TRUE(theme.states[static_cast<std::size_t>(engine::input::MouseCursorKind::Pointer)].has_value());
    EXPECT_TRUE(theme.states[static_cast<std::size_t>(engine::input::MouseCursorKind::Grab)].has_value());
    EXPECT_TRUE(theme.states[static_cast<std::size_t>(engine::input::MouseCursorKind::Dragging)].has_value());

    const auto default_hotspot =
        theme.states[static_cast<std::size_t>(engine::input::MouseCursorKind::Default)]->hotspot;
    EXPECT_EQ(theme.states[static_cast<std::size_t>(engine::input::MouseCursorKind::Pointer)]->hotspot.x,
              default_hotspot.x);
    EXPECT_EQ(theme.states[static_cast<std::size_t>(engine::input::MouseCursorKind::Pointer)]->hotspot.y,
              default_hotspot.y);
    EXPECT_EQ(theme.states[static_cast<std::size_t>(engine::input::MouseCursorKind::Grab)]->hotspot.x,
              default_hotspot.x);
    EXPECT_EQ(theme.states[static_cast<std::size_t>(engine::input::MouseCursorKind::Grab)]->hotspot.y,
              default_hotspot.y);
    EXPECT_EQ(theme.states[static_cast<std::size_t>(engine::input::MouseCursorKind::Dragging)]->hotspot.x,
              default_hotspot.x);
    EXPECT_EQ(theme.states[static_cast<std::size_t>(engine::input::MouseCursorKind::Dragging)]->hotspot.y,
              default_hotspot.y);
}

TEST(MouseCursorServiceTest, RejectsMissingRequiredState) {
    const auto root = nlohmann::json::parse(R"json({
        "sheet": "assets/farm-rpg/UI/HUD.png",
        "theme_id": "warm_light",
        "tile_size": 16,
        "scale": 2,
        "states": {
            "default": { "source": [0, 0, 16, 16], "hotspot": [1, 1] },
            "pointer": { "source": [16, 0, 16, 16], "hotspot": [1, 1] },
            "grab": { "source": [32, 0, 16, 16], "hotspot": [1, 1] }
        }
    })json");

    engine::input::MouseCursorTheme theme{};
    std::string error;
    EXPECT_FALSE(engine::input::parseMouseCursorThemeJson(root, theme, &error));
    EXPECT_NE(error.find("dragging"), std::string::npos);
}

TEST(MouseCursorServiceTest, ResolvePrefersOverrideOverUiCursor) {
    EXPECT_EQ(
        engine::input::resolveMouseCursorKind(engine::input::MouseCursorKind::Dragging,
                                             engine::input::MouseCursorKind::Text),
        engine::input::MouseCursorKind::Dragging);
    EXPECT_EQ(
        engine::input::resolveMouseCursorKind(std::nullopt, engine::input::MouseCursorKind::Text),
        engine::input::MouseCursorKind::Text);
}

TEST(MouseCursorServiceTest, BuildsNearestNeighborScaledBitmap) {
    engine::resource::DecodedImage image{};
    image.width = 2;
    image.height = 2;
    image.channels = 4;
    image.pixels = {
        1, 2, 3, 4,       10, 20, 30, 40,
        5, 6, 7, 8,       50, 60, 70, 80,
    };

    const engine::input::MouseCursorImageSpec spec{
        .source = engine::input::MouseCursorSourceRect{.x = 1, .y = 0, .width = 1, .height = 2},
        .hotspot = engine::input::MouseCursorHotspot{.x = 0, .y = 0},
    };

    const auto bitmap = engine::input::buildCursorBitmap(image, spec, 2);
    ASSERT_TRUE(bitmap.has_value());
    ASSERT_EQ(bitmap->width, 2);
    ASSERT_EQ(bitmap->height, 4);

    const std::vector<std::uint8_t> expected = {
        10, 20, 30, 40,   10, 20, 30, 40,
        10, 20, 30, 40,   10, 20, 30, 40,
        50, 60, 70, 80,   50, 60, 70, 80,
        50, 60, 70, 80,   50, 60, 70, 80,
    };
    EXPECT_EQ(bitmap->pixels, expected);
}
