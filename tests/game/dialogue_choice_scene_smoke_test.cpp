// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "../engine/render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace game::scene {
namespace {

TEST(DialogueChoiceSceneSmokeTest, DialogueChoiceSceneOwnsChoiceUiAndResultEvent) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/dialogue_choice_scene.h")
            .lexically_normal();
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/dialogue_choice_scene.cpp")
            .lexically_normal();
    const std::filesystem::path rml_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/dialogue_choice.rml")
            .lexically_normal();
    const std::filesystem::path rcss_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/dialogue_choice.rcss")
            .lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    ASSERT_TRUE(std::filesystem::exists(rml_path)) << rml_path;
    ASSERT_TRUE(std::filesystem::exists(rcss_path)) << rcss_path;

    const std::string header = test_source_utils::readTextFile(header_path);
    const std::string source = test_source_utils::readTextFile(source_path);
    const std::string rml = test_source_utils::readTextFile(rml_path);
    const std::string rcss = test_source_utils::readTextFile(rcss_path);
    ASSERT_FALSE(header.empty());
    ASSERT_FALSE(source.empty());
    ASSERT_FALSE(rml.empty());
    ASSERT_FALSE(rcss.empty());

    EXPECT_NE(header.find("class DialogueChoiceScene final"), std::string::npos);
    EXPECT_NE(header.find("DialogueChoiceRequestedEvent request_"), std::string::npos);
    EXPECT_NE(header.find("std::vector<ChoiceViewModel> choices_"), std::string::npos);
    EXPECT_NE(header.find("SceneUiCoverage uiCoverage() const override"), std::string::npos);

    const std::string init_block =
        test_source_utils::extractFunctionBlock(source, "bool DialogueChoiceScene::init()");
    const std::string init_ui_block =
        test_source_utils::extractFunctionBlock(source, "bool DialogueChoiceScene::initUI()");
    const std::string choose_block = test_source_utils::extractFunctionBlock(
        source, "void DialogueChoiceScene::onChoose(const int option_index)");
    const std::string cancel_block =
        test_source_utils::extractFunctionBlock(source, "void DialogueChoiceScene::onCancel()");
    ASSERT_FALSE(init_block.empty());
    ASSERT_FALSE(init_ui_block.empty());
    ASSERT_FALSE(choose_block.empty());
    ASSERT_FALSE(cancel_block.empty());

    EXPECT_NE(init_block.find("InputContextId::Dialogue"), std::string::npos);
    EXPECT_NE(init_block.find("setState(engine::core::State::Paused)"), std::string::npos);
    EXPECT_NE(init_ui_block.find("\"choose\""), std::string::npos);
    EXPECT_NE(init_ui_block.find("\"cancel\""), std::string::npos);
    EXPECT_NE(source.find("\"menu_cancel\"_hs"), std::string::npos);
    EXPECT_NE(source.find("DialogueChoiceScene::uiCoverage()"), std::string::npos);
    EXPECT_NE(source.find("SceneUiCoverage::HideUnderlyingSceneUi"), std::string::npos);
    EXPECT_NE(choose_block.find("DialogueChoiceSelectedEvent"), std::string::npos);
    EXPECT_NE(choose_block.find("requestPopScene()"), std::string::npos);
    EXPECT_NE(cancel_block.find("cancelled = true"), std::string::npos);

    EXPECT_NE(rml.find("data-model=\"dialogue_choice\""), std::string::npos);
    EXPECT_NE(rml.find("{{ prompt_text }}"), std::string::npos);
    EXPECT_NE(rml.find("data-for=\"choice : choices\""), std::string::npos);
    EXPECT_NE(rml.find("data-choice-index=\"{{ choice.option_index }}\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"choose\""), std::string::npos);
    EXPECT_NE(rml.find("id=\"dialogue-choice-cancel-button\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"cancel\""), std::string::npos);

    EXPECT_NE(rcss.find("display: block;"), std::string::npos);
    EXPECT_NE(rcss.find("#dialogue-choice-panel"), std::string::npos);
    EXPECT_NE(rcss.find(".dialogue-choice-option"), std::string::npos);
}

TEST(DialogueChoiceSceneSmokeTest, GameScenePushesDialogueChoiceSceneFromRequestEvent) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp")
            .lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("#include \"dialogue_choice_scene.h\""), std::string::npos);
    EXPECT_NE(source.find("sink<game::defs::DialogueChoiceRequestedEvent>()"), std::string::npos);

    const std::string handler_block = test_source_utils::extractFunctionBlock(
        source, "void GameScene::onDialogueChoiceRequested(const "
                "game::defs::DialogueChoiceRequestedEvent& evt)");
    ASSERT_FALSE(handler_block.empty());
    EXPECT_NE(handler_block.find("DialogueChoiceScene"), std::string::npos);
    EXPECT_NE(handler_block.find("isPaused()"), std::string::npos);
    EXPECT_NE(handler_block.find("isTransitionActive()"), std::string::npos);
    EXPECT_NE(handler_block.find("resolveDialogueChoiceSpeaker"), std::string::npos);
}

} // namespace
} // namespace game::scene
// NOLINTEND
