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
    EXPECT_NE(header.find("std::vector<game::ui::ShopBuyEntryViewModel> buy_entries_"), std::string::npos);
    EXPECT_NE(header.find("std::vector<game::ui::ShopSellEntryViewModel> sell_entries_"), std::string::npos);
    EXPECT_NE(header.find("ShopBuyPreview active_buy_preview_"), std::string::npos);
    EXPECT_NE(header.find("ShopSellPreview active_sell_preview_"), std::string::npos);
    EXPECT_NE(header.find("ShopMenuFocusArea current_focus_area_"), std::string::npos);
    EXPECT_NE(header.find("SceneUiCoverage uiCoverage() const override"), std::string::npos);

    const std::string init_block = test_source_utils::extractFunctionBlock(source, "bool ShopMenuScene::init()");
    const std::string init_ui_block = test_source_utils::extractFunctionBlock(source, "bool ShopMenuScene::initUI()");
    const std::string clean_block = test_source_utils::extractFunctionBlock(source, "void ShopMenuScene::clean()");
    const std::string connect_block =
        test_source_utils::extractFunctionBlock(source, "void ShopMenuScene::connectRuntimeListeners()");
    const std::string focus_block =
        test_source_utils::extractFunctionBlock(source, "void ShopMenuScene::syncFocusBindings()");
    const std::string buy_preview_block =
        test_source_utils::extractFunctionBlock(source, "void ShopMenuScene::refreshBuyPreview()");
    const std::string sell_preview_block =
        test_source_utils::extractFunctionBlock(source, "void ShopMenuScene::refreshSellPreview()");
    const std::string status_block =
        test_source_utils::extractFunctionBlock(source, "void ShopMenuScene::refreshStatusText()");
    ASSERT_FALSE(init_block.empty());
    ASSERT_FALSE(init_ui_block.empty());
    ASSERT_FALSE(clean_block.empty());
    ASSERT_FALSE(connect_block.empty());
    ASSERT_FALSE(focus_block.empty());
    ASSERT_FALSE(buy_preview_block.empty());
    ASSERT_FALSE(sell_preview_block.empty());
    ASSERT_FALSE(status_block.empty());

    EXPECT_NE(init_block.find("InputContextId::Menu"), std::string::npos);
    EXPECT_NE(init_ui_block.find("RegisterArray<decltype(buy_entries_)>()"), std::string::npos);
    EXPECT_NE(init_ui_block.find("RegisterArray<decltype(sell_entries_)>()"), std::string::npos);
    EXPECT_NE(init_ui_block.find("bindEvent("), std::string::npos);
    EXPECT_NE(init_ui_block.find("\"buy_entry_select\""), std::string::npos);
    EXPECT_NE(init_ui_block.find("\"sell_entry_select\""), std::string::npos);
    EXPECT_NE(init_ui_block.find("\"adjust_quantity\""), std::string::npos);
    EXPECT_NE(init_ui_block.find("\"switch_mode_buy\""), std::string::npos);
    EXPECT_NE(init_ui_block.find("\"switch_mode_sell\""), std::string::npos);
    EXPECT_NE(init_ui_block.find("\"confirm_trade\""), std::string::npos);
    EXPECT_NE(init_ui_block.find("bindSimpleEvent(constructor, \"close\""), std::string::npos);
    EXPECT_NE(init_ui_block.find("document_controller_.load(DOCUMENT_PATH)"), std::string::npos);
    EXPECT_NE(connect_block.find("\"menu_up\"_hs"), std::string::npos);
    EXPECT_NE(connect_block.find("\"menu_down\"_hs"), std::string::npos);
    EXPECT_NE(connect_block.find("\"menu_left\"_hs"), std::string::npos);
    EXPECT_NE(connect_block.find("\"menu_right\"_hs"), std::string::npos);
    EXPECT_NE(connect_block.find("\"menu_confirm\"_hs"), std::string::npos);
    EXPECT_NE(connect_block.find("\"menu_cancel\"_hs"), std::string::npos);
    EXPECT_NE(buy_preview_block.find("shop_transaction_service_->previewBuy"), std::string::npos);
    EXPECT_NE(sell_preview_block.find("shop_transaction_service_->previewSell"), std::string::npos);
    EXPECT_NE(source.find("shop_transaction_service_->commitBuy"), std::string::npos);
    EXPECT_NE(source.find("shop_transaction_service_->commitSell"), std::string::npos);
    EXPECT_NE(source.find("document_controller_.markDirty(\"buy_entries\")"), std::string::npos);
    EXPECT_NE(source.find("document_controller_.markDirty(\"sell_entries\")"), std::string::npos);
    EXPECT_NE(source.find("ShopMenuScene::uiCoverage()"), std::string::npos);
    EXPECT_NE(source.find("SceneUiCoverage::HideUnderlyingSceneUi"), std::string::npos);
    EXPECT_NE(focus_block.find("\"is_mode_toggle_focused\""), std::string::npos);
    EXPECT_NE(focus_block.find("\"is_entry_list_focused\""), std::string::npos);
    EXPECT_NE(focus_block.find("\"is_quantity_focused\""), std::string::npos);
    EXPECT_NE(focus_block.find("\"is_primary_action_focused\""), std::string::npos);
    EXPECT_NE(status_block.find("\"status_text\""), std::string::npos);
    EXPECT_NE(status_block.find("active_sell_preview_"), std::string::npos);
    EXPECT_NE(status_block.find("formatFocusStatus"), std::string::npos);
    EXPECT_NE(clean_block.find("context_.getInputManager().popContext();"), std::string::npos);
    EXPECT_NE(clean_block.find("context_.getGameState().setState(previous_state_);"), std::string::npos);

    EXPECT_NE(rml.find("data-model=\"shop_menu\""), std::string::npos);
    EXPECT_NE(rml.find("{{ shop_title }}"), std::string::npos);
    EXPECT_NE(rml.find("{{ shop_greeting }}"), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"switch_mode_buy\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"switch_mode_sell\""), std::string::npos);
    EXPECT_NE(rml.find("data-if=\"is_buy_mode"), std::string::npos);
    EXPECT_NE(rml.find("data-if=\"is_sell_mode"), std::string::npos);
    EXPECT_NE(rml.find("data-for=\"entry : buy_entries\""), std::string::npos);
    EXPECT_NE(rml.find("data-for=\"entry : sell_entries\""), std::string::npos);
    EXPECT_NE(rml.find("data-attr-data-shop-index=\"entry.index\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"buy_entry_select\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"sell_entry_select\""), std::string::npos);
    EXPECT_NE(rml.find("data-rml=\"entry.item_name\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"adjust_quantity(-1)\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"adjust_quantity(1)\""), std::string::npos);
    EXPECT_NE(rml.find("id=\"shop-primary-action-button\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"confirm_trade\""), std::string::npos);
    EXPECT_NE(rml.find("{{ primary_action_text }}"), std::string::npos);
    EXPECT_NE(rml.find("{{ detail_owned_label }}"), std::string::npos);
    EXPECT_NE(rml.find("{{ detail_name }}"), std::string::npos);
    EXPECT_NE(rml.find("{{ status_text }}"), std::string::npos);
    EXPECT_NE(rml.find("data-class-focused=\"is_mode_toggle_focused\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-focused=\"is_entry_list_focused\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-focused=\"is_quantity_focused\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-focused=\"is_primary_action_focused\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"close\""), std::string::npos);
    EXPECT_NE(rcss.find("display: block;"), std::string::npos);
    EXPECT_NE(rcss.find(".shop-mode-button"), std::string::npos);
    EXPECT_NE(rcss.find(".shop-buy-entry.selected"), std::string::npos);
    EXPECT_NE(rcss.find(".shop-sell-entry.selected"), std::string::npos);
    EXPECT_NE(rcss.find("#shop-sell-list"), std::string::npos);
    EXPECT_NE(rcss.find("#shop-mode-toggle.focused"), std::string::npos);
    EXPECT_NE(rcss.find("#shop-quantity-controls.focused"), std::string::npos);
    EXPECT_NE(rcss.find("#shop-quantity-controls"), std::string::npos);
    EXPECT_NE(rcss.find(".shop-action-button"), std::string::npos);
}

} // namespace
} // namespace game::scene
// NOLINTEND
