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

TEST(ShopMenuSceneSmokeTest, ShopMenuSceneOwnsMinimalSceneAndDocumentController) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/shop_menu_scene.h").lexically_normal();
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/shop_menu_scene.cpp").lexically_normal();
    const std::filesystem::path rml_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/shop_menu.rml").lexically_normal();
    const std::filesystem::path rcss_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/shop_menu.rcss").lexically_normal();
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

    EXPECT_NE(header.find("class ShopMenuScene final"), std::string::npos);
    EXPECT_NE(header.find("RmlDocumentController document_controller_"), std::string::npos);
    EXPECT_NE(header.find("std::string shop_id_"), std::string::npos);
    EXPECT_NE(header.find("ShopTransactionService* shop_transaction_service_"), std::string::npos);

    const std::string init_block = test_source_utils::extractFunctionBlock(source, "bool ShopMenuScene::init()");
    const std::string init_ui_block = test_source_utils::extractFunctionBlock(source, "bool ShopMenuScene::initUI()");
    const std::string clean_block = test_source_utils::extractFunctionBlock(source, "void ShopMenuScene::clean()");
    ASSERT_FALSE(init_block.empty());
    ASSERT_FALSE(init_ui_block.empty());
    ASSERT_FALSE(clean_block.empty());

    EXPECT_NE(init_block.find("InputContextId::Menu"), std::string::npos);
    EXPECT_NE(init_block.find("\"menu_cancel\"_hs"), std::string::npos);
    EXPECT_NE(init_ui_block.find("bindSimpleEvent(constructor, \"close\""), std::string::npos);
    EXPECT_NE(init_ui_block.find("document_controller_.load(DOCUMENT_PATH)"), std::string::npos);
    EXPECT_NE(clean_block.find("context_.getInputManager().popContext();"), std::string::npos);
    EXPECT_NE(clean_block.find("context_.getGameState().setState(previous_state_);"), std::string::npos);

    EXPECT_NE(rml.find("data-model=\"shop_menu\""), std::string::npos);
    EXPECT_NE(rml.find("{{ shop_title }}"), std::string::npos);
    EXPECT_NE(rml.find("{{ shop_greeting }}"), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"close\""), std::string::npos);
    EXPECT_NE(rcss.find("display: block;"), std::string::npos);
    EXPECT_NE(rcss.find("tab-index: auto;"), std::string::npos);
}

} // namespace
} // namespace game::scene
// NOLINTEND
