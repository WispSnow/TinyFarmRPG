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
    EXPECT_NE(source.find("setLocalizedMessage(\"pause.message.saving\", false"), std::string::npos)
        << "PauseMenuScene should show a localized in-progress saving message after async save starts.";
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
    EXPECT_NE(scene_source.find("setLocalizedMessage(\"pause.message.save_in_progress\", true"), std::string::npos)
        << "PauseMenuScene should give localized user feedback when save is still running.";
}

TEST(PauseMenuSceneAsyncSaveUiTest, DynamicStringsRefreshOnLanguageChange) {
    const std::filesystem::path scene_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/pause_menu_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(scene_source_path)) << scene_source_path;

    const std::string scene_source = readTextFile(scene_source_path);
    ASSERT_FALSE(scene_source.empty());

    EXPECT_NE(scene_source.find("sink<game::defs::LanguageChangedEvent>()"), std::string::npos)
        << "PauseMenuScene should subscribe language changes for dynamic RmlUi bindings.";
    EXPECT_NE(scene_source.find("refreshLocalizedBindings()"), std::string::npos)
        << "PauseMenuScene should rebuild dynamic labels and messages after a language change.";
    EXPECT_NE(scene_source.find("\"pause.value.music\""), std::string::npos)
        << "Music volume label should be generated from an i18n key.";
    EXPECT_NE(scene_source.find("\"pause.value.sound\""), std::string::npos)
        << "SFX volume label should be generated from an i18n key.";
    EXPECT_NE(scene_source.find("\"pause.value.speed\""), std::string::npos)
        << "Time scale label should be generated from an i18n key.";
}

TEST(PauseMenuSceneAsyncSaveUiTest, ReloadsScriptHostAfterSuccessfulLoad) {
    const std::filesystem::path scene_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/pause_menu_scene.cpp").lexically_normal();
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/pause_menu_scene.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(scene_source_path)) << scene_source_path;
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;

    const std::string scene_source = readTextFile(scene_source_path);
    const std::string header_source = readTextFile(header_path);
    ASSERT_FALSE(scene_source.empty());
    ASSERT_FALSE(header_source.empty());

    const auto load_pos = scene_source.find("save_service_->loadFromFile");
    const auto settings_pos = scene_source.find("user_settings_service_->applyAll()");
    const auto reload_pos = scene_source.find("script_host_->reload()");

    ASSERT_NE(load_pos, std::string::npos);
    ASSERT_NE(settings_pos, std::string::npos);
    ASSERT_NE(reload_pos, std::string::npos);

    EXPECT_LT(load_pos, reload_pos)
        << "Pause-menu load must apply saved state before refreshing Lua callbacks.";
    EXPECT_LT(settings_pos, reload_pos)
        << "Global user settings should be restored before Lua bootstrap observes runtime services.";
    EXPECT_NE(header_source.find("engine::script::ScriptHost* script_host_"), std::string::npos);
}

TEST(PauseMenuSceneAsyncSaveUiTest, ExposesLanguageStepper) {
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

    EXPECT_NE(scene_source.find("constructor.Bind(\"language_text\""), std::string::npos);
    EXPECT_NE(scene_source.find("constructor.Bind(\"can_change_language\""), std::string::npos);
    EXPECT_NE(scene_source.find("bindSimpleEvent(constructor, \"language_down\""), std::string::npos);
    EXPECT_NE(scene_source.find("bindSimpleEvent(constructor, \"language_up\""), std::string::npos);
    EXPECT_NE(scene_source.find("void PauseMenuScene::adjustLanguage"), std::string::npos);
    EXPECT_NE(rml_source.find("{{ language_text }}"), std::string::npos);
    EXPECT_NE(rml_source.find("data-event-click=\"language_down\""), std::string::npos);
    EXPECT_NE(rml_source.find("data-event-click=\"language_up\""), std::string::npos);
    EXPECT_NE(rml_source.find("data-attrif-disabled=\"!can_change_language\""), std::string::npos);
}

TEST(PauseMenuSceneAsyncSaveUiTest, ExposesDeleteSlotAction) {
    const std::filesystem::path scene_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/pause_menu_scene.cpp").lexically_normal();
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/pause_menu_scene.h").lexically_normal();
    const std::filesystem::path rml_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/pause_menu.rml").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(scene_source_path)) << scene_source_path;
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(rml_path)) << rml_path;

    const std::string scene_source = readTextFile(scene_source_path);
    const std::string header_source = readTextFile(header_path);
    const std::string rml_source = readTextFile(rml_path);
    ASSERT_FALSE(scene_source.empty());
    ASSERT_FALSE(header_source.empty());
    ASSERT_FALSE(rml_source.empty());

    EXPECT_NE(header_source.find("bool can_delete_"), std::string::npos);
    EXPECT_NE(header_source.find("void onDeleteClicked()"), std::string::npos);
    EXPECT_NE(scene_source.find("constructor.Bind(\"can_delete\""), std::string::npos);
    EXPECT_NE(scene_source.find("bindSimpleEvent(constructor, \"delete_save\""), std::string::npos);
    EXPECT_NE(scene_source.find("game::save::SaveService::deleteSlot"), std::string::npos);
    EXPECT_NE(scene_source.find("SaveSlotSelectScene::Mode::Delete"), std::string::npos);
    EXPECT_NE(rml_source.find("data-event-click=\"delete_save\""), std::string::npos);
    EXPECT_NE(rml_source.find("data-attrif-disabled=\"!can_delete\""), std::string::npos);
}

} // namespace
} // namespace game::scene
// NOLINTEND
