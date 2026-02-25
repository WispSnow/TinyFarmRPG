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
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/pause_menu_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("back_to_title_button_->setEnabled(!saving);"), std::string::npos)
        << "PauseMenuScene should disable Back to Title button while async save is running.";
    EXPECT_NE(source.find("if (save_service_ && save_service_->isSaving())"), std::string::npos)
        << "BackToTitle click handler should defensively reject scene replacement during save.";
    EXPECT_NE(source.find("setMessage(\"Save in progress\", true);"), std::string::npos)
        << "PauseMenuScene should give user feedback when save is still running.";
}

} // namespace
} // namespace game::scene
// NOLINTEND
