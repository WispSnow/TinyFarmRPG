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

TEST(PauseMenuSceneAsyncSaveUiTest, UsesAsyncSaveAndEventDrivenCompletionHandling) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/pause_menu_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("save_service_->saveToFileAsync"), std::string::npos)
        << "PauseMenuScene should trigger async save instead of synchronous saveToFile.";
    EXPECT_NE(source.find("sink<game::defs::AsyncSaveCompletedEvent>()"), std::string::npos)
        << "PauseMenuScene should subscribe async save completion event on dispatcher.";
    EXPECT_NE(source.find("onAsyncSaveCompleted"), std::string::npos)
        << "PauseMenuScene should handle async save terminal state in event callback.";
    EXPECT_NE(source.find("setMessage(\"Saving...\", false);"), std::string::npos)
        << "PauseMenuScene should show an in-progress saving message after async save starts.";
}

TEST(PauseMenuSceneAsyncSaveUiTest, DisablesBackToTitleWhileSaving) {
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

    EXPECT_NE(scene_source.find("updateBoundBool(can_back_title_, !saving)"), std::string::npos)
        << "PauseMenuScene should publish Back to Title enable state through the RmlUi data model.";
    EXPECT_NE(scene_source.find("document_controller_.markDirty(\"can_back_title\")"), std::string::npos)
        << "PauseMenuScene should mark can_back_title dirty after save state changes.";
    EXPECT_NE(rml_source.find("data-event-click=\"back_to_title\""), std::string::npos)
        << "Pause menu RML should expose a Back to Title action button.";
    EXPECT_NE(rml_source.find("data-attrif-disabled=\"!can_back_title\""), std::string::npos)
        << "Pause menu Title button should bind disabled attr to can_back_title.";
    EXPECT_NE(scene_source.find("if (save_service_ && save_service_->isSaving())"), std::string::npos)
        << "BackToTitle click handler should defensively reject scene replacement during save.";
    EXPECT_NE(scene_source.find("setMessage(\"Save in progress\", true);"), std::string::npos)
        << "PauseMenuScene should give user feedback when save is still running.";
}

} // namespace
} // namespace game::scene
// NOLINTEND
