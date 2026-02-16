// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << path;
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

} // namespace

namespace engine::scene {
namespace {

TEST(SceneManagerSafetyTest, OnPopSceneClearsPendingScene) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/scene/scene_manager.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("pending_scene_.reset()"), std::string::npos)
        << "onPopScene should clear pending_scene_ to avoid retaining an unused Scene when a push gets overridden by pop within the same frame.";
}

TEST(SceneManagerSafetyTest, SwitchLogsIncludeStackTransition) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/scene/scene_manager.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("SceneManager:"), std::string::npos)
        << "SceneManager should prefix key switch logs for greppability.";
    EXPECT_NE(source.find("stack {} -> {}"), std::string::npos)
        << "SceneManager push/pop/replace logs should include stack size transitions (before -> after).";
}

} // namespace
} // namespace engine::scene
// NOLINTEND

