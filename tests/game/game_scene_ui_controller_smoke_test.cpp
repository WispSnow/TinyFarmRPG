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

TEST(GameSceneUiControllerSmokeTest, GameSceneOwnsAndBuildsUiControllerAndSceneOverlays) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.h").lexically_normal();
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string header = test_source_utils::readTextFile(header_path);
    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(header.empty()) << "无法读取: " << header_path;
    ASSERT_FALSE(source.empty()) << "无法读取: " << source_path;

    EXPECT_NE(header.find("std::unique_ptr<game::ui::GameSceneUiController> ui_controller_{};"), std::string::npos);
    EXPECT_NE(header.find("std::unique_ptr<game::ui::GameOverlay> game_overlay_{};"), std::string::npos);
    EXPECT_NE(header.find("std::unique_ptr<game::ui::GameInputPromptOverlay> input_prompt_overlay_{};"),
              std::string::npos);

    const std::string init_ui_block = test_source_utils::extractFunctionBlock(source, "bool GameScene::initUI()");
    ASSERT_FALSE(init_ui_block.empty());
    EXPECT_NE(init_ui_block.find("std::make_unique<game::ui::GameSceneUiController>("), std::string::npos);
    EXPECT_NE(init_ui_block.find("services_->rpg_catalog.get()"), std::string::npos);
    EXPECT_NE(init_ui_block.find("ui_controller_->init()"), std::string::npos);
    EXPECT_NE(init_ui_block.find("std::make_unique<game::ui::GameOverlay>("), std::string::npos);
    EXPECT_NE(init_ui_block.find("std::make_unique<game::ui::GameInputPromptOverlay>("), std::string::npos);
    EXPECT_NE(init_ui_block.find("systems_->map_transition_system->setFadeOverlay(ui_controller_->screenFade());"),
              std::string::npos);
}

TEST(GameSceneUiControllerSmokeTest, GameSceneDelegatesUiLifecycleAndActionsToController) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string update_block = test_source_utils::extractFunctionBlock(content, "void GameScene::update(float delta_time)");
    const std::string prepare_ui_block = test_source_utils::extractFunctionBlock(content, "void GameScene::prepareUi(float interpolation_alpha)");
    const std::string clean_block = test_source_utils::extractFunctionBlock(content, "void GameScene::clean()");
    const std::string hotbar_block = test_source_utils::extractFunctionBlock(content, "bool GameScene::onHotbarToggle()");
    const std::string prompt_block = test_source_utils::extractFunctionBlock(content, "bool GameScene::onTogglePromptBar()");

    ASSERT_FALSE(update_block.empty());
    ASSERT_FALSE(prepare_ui_block.empty());
    ASSERT_FALSE(clean_block.empty());
    ASSERT_FALSE(hotbar_block.empty());
    ASSERT_FALSE(prompt_block.empty());

    EXPECT_NE(update_block.find("ui_controller_->update(delta_time);"), std::string::npos);
    EXPECT_NE(update_block.find("input_prompt_overlay_->update();"), std::string::npos);
    EXPECT_NE(prepare_ui_block.find("ui_controller_->refreshAnchoredWidgets(camera, clamped_alpha);"), std::string::npos);
    EXPECT_NE(clean_block.find("game_overlay_.reset();"), std::string::npos);
    EXPECT_NE(clean_block.find("input_prompt_overlay_.reset();"), std::string::npos);
    EXPECT_NE(clean_block.find("ui_controller_.reset();"), std::string::npos);
    EXPECT_NE(hotbar_block.find("ui_controller_->toggleHotbar();"), std::string::npos);
    EXPECT_NE(prompt_block.find("input_prompt_overlay_->toggleVisible();"), std::string::npos);
}

TEST(GameSceneUiControllerSmokeTest, GameScenePassesQuestCatalogWhenOpeningInventoryMenu) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string inventory_block = test_source_utils::extractFunctionBlock(content, "bool GameScene::onInventoryToggle()");
    ASSERT_FALSE(inventory_block.empty());

    EXPECT_NE(inventory_block.find("InventoryMenuScene"), std::string::npos);
    EXPECT_NE(inventory_block.find("services_->item_catalog.get()"), std::string::npos);
    EXPECT_NE(inventory_block.find("services_->quest_catalog.get()"), std::string::npos);
}

} // namespace
} // namespace game::scene

namespace game::ui {
namespace {

TEST(GameSceneUiControllerSmokeTest, ControllerOwnsHudDialogueAndFadeComposition) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/ui/game_scene_ui_controller.h").lexically_normal();
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/ui/game_scene_ui_controller.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string header = test_source_utils::readTextFile(header_path);
    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(header.empty()) << "无法读取: " << header_path;
    ASSERT_FALSE(source.empty()) << "无法读取: " << source_path;

    EXPECT_NE(header.find("std::unique_ptr<game::ui::HotbarUI> hotbar_ui_{};"), std::string::npos);
    EXPECT_NE(header.find("std::unique_ptr<game::ui::DialogueBoxView> dialogue_box_{};"),
              std::string::npos);
    EXPECT_NE(header.find("std::array<std::unique_ptr<game::ui::FloatingNoticeView>, 2> floating_notices_{};"),
              std::string::npos);
    EXPECT_NE(header.find("std::unique_ptr<game::ui::ItemTooltipUI> item_tooltip_ui_{};"), std::string::npos);
    EXPECT_NE(header.find("std::unique_ptr<game::ui::TimeClockHud> time_clock_hud_{};"), std::string::npos);
    EXPECT_NE(header.find("std::unique_ptr<engine::ui::rmlui::RmlScreenFade> rml_screen_fade_{};"),
              std::string::npos);

    const std::string init_block =
        test_source_utils::extractFunctionBlock(source, "bool GameSceneUiController::init()");
    ASSERT_FALSE(init_block.empty());
    EXPECT_NE(init_block.find("time_clock_hud_ = std::make_unique<game::ui::TimeClockHud>("), std::string::npos);
    EXPECT_NE(init_block.find("hotbar_ui_ = std::make_unique<game::ui::HotbarUI>("), std::string::npos);
    EXPECT_NE(init_block.find("item_tooltip_ui_ = std::make_unique<game::ui::ItemTooltipUI>("), std::string::npos);
    EXPECT_NE(init_block.find("dialogue_box_ = std::make_unique<game::ui::DialogueBoxView>("),
              std::string::npos);
    EXPECT_NE(init_block.find("dialogue_controller_ = std::make_unique<game::ui::DialoguePresentationController>("),
              std::string::npos);
    EXPECT_NE(init_block.find("rml_screen_fade_ = std::make_unique<engine::ui::rmlui::RmlScreenFade>("),
              std::string::npos);
}

TEST(GameSceneUiControllerSmokeTest, InputPromptOverlayOwnsPromptDocumentAndVisibilityModel) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/ui/game_input_prompt_overlay.h").lexically_normal();
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/ui/game_input_prompt_overlay.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string header = test_source_utils::readTextFile(header_path);
    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(header.empty()) << "无法读取: " << header_path;
    ASSERT_FALSE(source.empty()) << "无法读取: " << source_path;

    EXPECT_NE(header.find("void toggleVisible();"), std::string::npos);
    EXPECT_NE(header.find("bool visible_{false};"), std::string::npos);
    EXPECT_NE(source.find("document_controller_.load(DOCUMENT_PATH)"), std::string::npos);
    EXPECT_NE(source.find("game_input_prompt_overlay.rml"), std::string::npos);
    EXPECT_NE(source.find("constructor.Bind(\"visible\", &visible_);"), std::string::npos);
    EXPECT_NE(source.find("refresh_prompt(\"toggle_prompt_text\""), std::string::npos);
}

TEST(GameSceneUiControllerSmokeTest, GameOverlayOwnsMenuButtonDocumentAndCallback) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/ui/game_overlay.h").lexically_normal();
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/ui/game_overlay.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string header = test_source_utils::readTextFile(header_path);
    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(header.empty()) << "无法读取: " << header_path;
    ASSERT_FALSE(source.empty()) << "无法读取: " << source_path;

    EXPECT_NE(header.find("class GameOverlay final"), std::string::npos);
    EXPECT_NE(source.find("game_overlay.rml"), std::string::npos);
    EXPECT_NE(source.find("bindSimpleEvent(constructor, \"menu\""), std::string::npos);
}

} // namespace
} // namespace game::ui
// NOLINTEND
