// NOLINTBEGIN
#include <gtest/gtest.h>

#include "engine/loader/level_loader.h"

#include <filesystem>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::loader {
namespace {

TEST(LevelLoaderAsyncStageTest, WorkerStagePreprocessesLevelData) {
    const std::filesystem::path level_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/maps/home_exterior.tmj").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(level_path)) << level_path;

    const auto preprocess = LevelLoader::preprocessLevelDataWorker(level_path.string());
    ASSERT_TRUE(preprocess.success) << preprocess.error;
    EXPECT_EQ(preprocess.data.level_path, level_path.string());
    EXPECT_FALSE(preprocess.data.tilesets.empty());
    EXPECT_FALSE(preprocess.data.texture_paths.empty());
}

TEST(LevelLoaderAsyncStageTest, WorkerStageRejectsEmptyLevelPath) {
    const auto preprocess = LevelLoader::preprocessLevelDataWorker({});
    EXPECT_FALSE(preprocess.success);
    EXPECT_FALSE(preprocess.error.empty());
}

} // namespace
} // namespace engine::loader
// NOLINTEND
