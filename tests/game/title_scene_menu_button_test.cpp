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

TEST(TitleSceneMenuButtonTest, BindsRmlMenuButtonToPauseMenuScene) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/title_scene.cpp").lexically_normal();
    const std::filesystem::path rml_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/title.rml").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    ASSERT_TRUE(std::filesystem::exists(rml_path)) << rml_path;

    const std::string source = readTextFile(source_path);
    const std::string rml_source = readTextFile(rml_path);
    ASSERT_FALSE(source.empty());
    ASSERT_FALSE(rml_source.empty());

    EXPECT_NE(source.find("event_bridge_.on(\"menu\""), std::string::npos)
        << "TitleScene should bind the menu command through the RmlUi event bridge.";
    EXPECT_NE(rml_source.find("data-command=\"menu\""), std::string::npos)
        << "TitleScene RML should expose a menu button command.";
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
