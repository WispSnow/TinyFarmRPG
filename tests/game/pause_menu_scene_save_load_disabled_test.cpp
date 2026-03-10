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
    const std::filesystem::path scene_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/pause_menu_scene.cpp").lexically_normal();
    const std::filesystem::path rml_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/pause_menu.rml").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(scene_source_path)) << scene_source_path;
    ASSERT_TRUE(std::filesystem::exists(rml_path)) << rml_path;

    const std::string scene_source = readTextFile(scene_source_path);
    const std::string rml_source = readTextFile(rml_path);
    ASSERT_FALSE(scene_source.empty());
    ASSERT_FALSE(rml_source.empty());

    EXPECT_NE(scene_source.find("const bool has_save_service = (save_service_ != nullptr);"), std::string::npos)
        << "PauseMenuScene should derive enable state from save_service_ presence.";
    EXPECT_NE(scene_source.find("updateBoundBool(can_save_, has_save_service && !saving)"), std::string::npos)
        << "PauseMenuScene should publish Save enable state through the RmlUi data model.";
    EXPECT_NE(scene_source.find("updateBoundBool(can_load_, has_save_service && !saving)"), std::string::npos)
        << "PauseMenuScene should publish Load enable state through the RmlUi data model.";
    EXPECT_NE(scene_source.find("data_bridge_.markDirty(\"can_save\")"), std::string::npos)
        << "PauseMenuScene should mark can_save dirty after state changes.";
    EXPECT_NE(scene_source.find("data_bridge_.markDirty(\"can_load\")"), std::string::npos)
        << "PauseMenuScene should mark can_load dirty after state changes.";
    EXPECT_NE(scene_source.find("refreshSaveActionButtons();"), std::string::npos)
        << "PauseMenuScene should refresh Save/Load enable state via unified helper.";
    EXPECT_NE(rml_source.find("data-command=\"save\""), std::string::npos)
        << "Pause menu RML should expose a Save action button.";
    EXPECT_NE(rml_source.find("data-command=\"load\""), std::string::npos)
        << "Pause menu RML should expose a Load action button.";
    EXPECT_NE(rml_source.find("data-attrif-disabled=\"!can_save\""), std::string::npos)
        << "Pause menu Save button should bind disabled attr to can_save.";
    EXPECT_NE(rml_source.find("data-attrif-disabled=\"!can_load\""), std::string::npos)
        << "Pause menu Load button should bind disabled attr to can_load.";
}

} // namespace
} // namespace game::scene
// NOLINTEND
