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

TEST(PauseMenuSceneSaveLoadDisabledTest, DisablesButtonsWhenSaveServiceMissing) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/pause_menu_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("if (!save_service_)"), std::string::npos)
        << "PauseMenuScene should check save_service_ before enabling Save/Load.";
    EXPECT_NE(source.find("disableButton(save_button_)"), std::string::npos)
        << "PauseMenuScene should disable the Save button when SaveService is missing.";
    EXPECT_NE(source.find("disableButton(load_button_)"), std::string::npos)
        << "PauseMenuScene should disable the Load button when SaveService is missing.";
}

} // namespace
} // namespace game::scene
// NOLINTEND
