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

TEST(SceneManagerSafetyTest, SyncsRmlUiVisibilityForFullScreenScenes) {
    const std::filesystem::path scene_header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/scene/scene.h").lexically_normal();
    const std::filesystem::path manager_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/scene/scene_manager.cpp").lexically_normal();
    const std::filesystem::path battle_header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.h").lexically_normal();
    const std::filesystem::path battle_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(scene_header_path)) << scene_header_path;
    ASSERT_TRUE(std::filesystem::exists(manager_source_path)) << manager_source_path;
    ASSERT_TRUE(std::filesystem::exists(battle_header_path)) << battle_header_path;
    ASSERT_TRUE(std::filesystem::exists(battle_source_path)) << battle_source_path;

    const std::string scene_header = readTextFile(scene_header_path);
    const std::string manager_source = readTextFile(manager_source_path);
    const std::string battle_header = readTextFile(battle_header_path);
    const std::string battle_source = readTextFile(battle_source_path);
    ASSERT_FALSE(scene_header.empty());
    ASSERT_FALSE(manager_source.empty());
    ASSERT_FALSE(battle_header.empty());
    ASSERT_FALSE(battle_source.empty());

    EXPECT_NE(scene_header.find("enum class SceneUiCoverage"), std::string::npos);
    EXPECT_NE(scene_header.find("HideUnderlyingSceneUi"), std::string::npos);
    EXPECT_NE(scene_header.find("virtual SceneUiCoverage uiCoverage() const"), std::string::npos);
    EXPECT_NE(manager_source.find("setVisibleSceneOwners"), std::string::npos);
    EXPECT_NE(manager_source.find("uiCoverage() == SceneUiCoverage::HideUnderlyingSceneUi"), std::string::npos);
    EXPECT_NE(battle_header.find("SceneUiCoverage uiCoverage() const override"), std::string::npos);
    EXPECT_NE(battle_source.find("SceneUiCoverage::HideUnderlyingSceneUi"), std::string::npos);
}

} // namespace
} // namespace engine::scene
// NOLINTEND
