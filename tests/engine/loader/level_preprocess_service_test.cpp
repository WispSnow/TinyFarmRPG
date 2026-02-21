// NOLINTBEGIN
#include <gtest/gtest.h>

#include "engine/loader/level_preprocess_service.h"

#include <algorithm>
#include <filesystem>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::loader {
namespace {

TEST(LevelPreprocessServiceTest, CollectsTilesetAndTexturePathsFromLevel) {
    const std::filesystem::path level_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/maps/home_exterior.tmj").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(level_path)) << level_path;

    const auto result = LevelPreprocessService::preprocessLevel(level_path.string());
    ASSERT_TRUE(result.success) << result.error;
    EXPECT_EQ(result.data.level_path, level_path.string());
    EXPECT_FALSE(result.data.tilesets.empty());
    EXPECT_FALSE(result.data.tileset_paths.empty());
    EXPECT_FALSE(result.data.texture_paths.empty());
    EXPECT_LE(result.data.tilesets.size(), result.data.tileset_paths.size());
    EXPECT_TRUE(std::is_sorted(result.data.tileset_paths.begin(), result.data.tileset_paths.end()));
    EXPECT_TRUE(std::is_sorted(result.data.texture_paths.begin(), result.data.texture_paths.end()));

    for (const auto& tileset : result.data.tilesets) {
        EXPECT_GT(tileset.first_gid, 0);
        EXPECT_FALSE(tileset.path.empty());
    }
}

} // namespace
} // namespace engine::loader
// NOLINTEND
