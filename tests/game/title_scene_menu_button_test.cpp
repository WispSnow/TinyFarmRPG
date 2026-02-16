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

namespace game::scene {
namespace {

TEST(TitleSceneMenuButtonTest, AddsMenuButtonPreset) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/title_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("context_, \"menu\""), std::string::npos)
        << "TitleScene should create a menu button using the 'menu' preset.";
    EXPECT_NE(source.find("PauseMenuScene"), std::string::npos)
        << "TitleScene should open PauseMenuScene from the menu button.";
}

TEST(TitleSceneMenuButtonTest, StartUsesTitleGameTime) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/title_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("GameScene\", context_, title_game_time_"), std::string::npos)
        << "TitleScene should pass title_game_time_ into GameScene when starting a new game.";
}

} // namespace
} // namespace game::scene
// NOLINTEND
