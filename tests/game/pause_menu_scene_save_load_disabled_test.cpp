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

    EXPECT_NE(source.find("const bool has_save_service = (save_service_ != nullptr);"), std::string::npos)
        << "PauseMenuScene should derive enable state from save_service_ presence.";
    EXPECT_NE(source.find("save_button_->setEnabled(has_save_service && !saving);"), std::string::npos)
        << "PauseMenuScene should disable Save when SaveService is missing.";
    EXPECT_NE(source.find("load_button_->setEnabled(has_save_service && !saving);"), std::string::npos)
        << "PauseMenuScene should disable Load when SaveService is missing.";
    EXPECT_NE(source.find("refreshSaveActionButtons();"), std::string::npos)
        << "PauseMenuScene should refresh Save/Load enable state via unified helper.";
}

} // namespace
} // namespace game::scene
// NOLINTEND
