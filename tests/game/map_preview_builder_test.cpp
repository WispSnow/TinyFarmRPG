#include <gtest/gtest.h>

#include "game/ui/map_preview_builder.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace game::ui {
namespace {

[[nodiscard]] std::filesystem::path mapPath(std::string_view file_name) {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/maps" / file_name).lexically_normal();
}

[[nodiscard]] std::filesystem::path makeTempRoot() {
    const auto root = std::filesystem::temp_directory_path() /
                      ("map_preview_builder_" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    return root;
}

void writeTextFile(const std::filesystem::path& path, std::string_view content) {
    std::ofstream file(path);
    ASSERT_TRUE(file.is_open()) << path;
    file << content;
}

TEST(MapPreviewBuilderTest, BuildsPreviewFromVisibleTileLayers) {
    const std::filesystem::path path = mapPath("home_exterior.tmj");
    ASSERT_TRUE(std::filesystem::exists(path)) << path;

    MapPreviewBuilder builder;
    const MapPreviewBuildResult result = builder.buildPreview("home_exterior", path.string());

    ASSERT_TRUE(result.valid());
    EXPECT_EQ(result.map_pixel_size.x, 560);
    EXPECT_EQ(result.map_pixel_size.y, 400);
    EXPECT_EQ(result.image.width, 560);
    EXPECT_EQ(result.image.height, 400);
    EXPECT_EQ(result.image.channels, 4);
    EXPECT_FALSE(result.from_cache);

    const bool has_visible_alpha = std::ranges::any_of(result.image.pixels, [](std::uint8_t value) {
        return value != 0;
    });
    EXPECT_TRUE(has_visible_alpha);
}

TEST(MapPreviewBuilderTest, BuildsPreviewFromObjectLayerTileImages) {
    const std::filesystem::path house_image =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/farm-rpg/Exterior/Houses/3.png").lexically_normal();
    if (!std::filesystem::exists(house_image)) {
        GTEST_SKIP() << "Missing integration fixture asset: " << house_image;
    }

    const std::filesystem::path root = makeTempRoot();
    const std::filesystem::path tileset_path = root / "house.tsj";
    const std::filesystem::path map_path = root / "house_object.tmj";

    writeTextFile(
        tileset_path,
        R"json({
  "type": "tileset",
  "tilewidth": 128,
  "tileheight": 112,
  "tilecount": 1,
  "columns": 0,
  "tiles": [
    {
      "id": 0,
      "image": ")json" + house_image.string() + R"json(",
      "imagewidth": 128,
      "imageheight": 112,
      "width": 128,
      "height": 112
    }
  ]
})json");
    writeTextFile(
        map_path,
        R"json({
  "type": "map",
  "width": 8,
  "height": 7,
  "tilewidth": 16,
  "tileheight": 16,
  "backgroundcolor": "#000000",
  "tilesets": [
    { "firstgid": 1, "source": "house.tsj" }
  ],
  "layers": [
    {
      "type": "objectgroup",
      "name": "main",
      "visible": true,
      "opacity": 1,
      "objects": [
        { "id": 1, "gid": 1, "x": 0, "y": 112, "width": 128, "height": 112, "visible": true }
      ]
    }
  ]
})json");

    MapPreviewBuilder builder;
    const MapPreviewBuildResult result = builder.buildPreview("house_object_test", map_path.string());

    ASSERT_TRUE(result.valid());
    EXPECT_EQ(result.image.width, 128);
    EXPECT_EQ(result.image.height, 112);
    bool has_non_black_pixel = false;
    for (std::size_t i = 0; i + 3U < result.image.pixels.size(); i += 4U) {
        if (result.image.pixels[i + 0U] != 0 || result.image.pixels[i + 1U] != 0 ||
            result.image.pixels[i + 2U] != 0) {
            has_non_black_pixel = true;
            break;
        }
    }
    EXPECT_TRUE(has_non_black_pixel);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

TEST(MapPreviewBuilderTest, ReusesPreviewCacheForSameMap) {
    const std::filesystem::path path = mapPath("home_interior.tmj");
    ASSERT_TRUE(std::filesystem::exists(path)) << path;

    MapPreviewBuilder builder;
    const MapPreviewBuildResult first = builder.buildPreview("home_interior", path.string());
    const MapPreviewBuildResult second = builder.buildPreview("home_interior", path.string());

    ASSERT_TRUE(first.valid());
    ASSERT_TRUE(second.valid());
    EXPECT_FALSE(first.from_cache);
    EXPECT_TRUE(second.from_cache);
    EXPECT_EQ(second.map_pixel_size.x, 240);
    EXPECT_EQ(second.map_pixel_size.y, 240);
}

TEST(MapPreviewBuilderTest, MissingMapReturnsInvalidResult) {
    MapPreviewBuilder builder;
    const MapPreviewBuildResult result = builder.buildPreview("missing", "assets/maps/missing.tmj");

    EXPECT_FALSE(result.valid());
}

} // namespace
} // namespace game::ui
