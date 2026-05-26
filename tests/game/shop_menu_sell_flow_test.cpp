// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include <entt/core/hashed_string.hpp>

#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/data/shop_data.h"
#include "game/runtime/localization_service.h"
#include "game/ui/shop_menu_support.h"

#include "../engine/render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace game::scene {
namespace {

[[nodiscard]] game::data::ItemCatalog loadProjectItemCatalog() {
    game::data::ItemCatalog catalog;
    const auto config_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/data/item_config.json").lexically_normal();
    EXPECT_TRUE(catalog.loadItemConfig(config_path.string()));
    return catalog;
}

[[nodiscard]] game::runtime::LocalizationService loadEnglishLocalization() {
    game::runtime::LocalizationService localization;
    const auto manifest_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/i18n/languages.json").lexically_normal();
    EXPECT_TRUE(localization.loadLanguageIndex(manifest_path.string()));
    EXPECT_TRUE(localization.setLanguage("en-US"));
    return localization;
}

TEST(ShopMenuSellSupportTest, ResolveSellQuantityUiMaxTracksCurrentSlotCount) {
    game::component::ItemStack stack{};
    stack.item_id_ = entt::hashed_string{"potion"}.value();
    stack.count_ = 7;
    EXPECT_EQ(game::ui::resolveSellQuantityUiMax(stack), 7);

    stack.count_ = 1;
    EXPECT_EQ(game::ui::resolveSellQuantityUiMax(stack), 1);

    stack.count_ = 0;
    EXPECT_EQ(game::ui::resolveSellQuantityUiMax(stack), 1);
}

TEST(ShopMenuSellSupportTest, PopulateShopSellEntryViewModelFormatsFieldsAndDisabledState) {
    const auto item_catalog = loadProjectItemCatalog();
    const auto localization = loadEnglishLocalization();

    game::component::ItemStack stack{};
    stack.item_id_ = entt::hashed_string{"potion"}.value();
    stack.count_ = 3;

    game::data::ShopSellRuleData sell_rule{};
    sell_rule.item_id_ = "potion";
    sell_rule.item_id_hash_ = stack.item_id_;
    sell_rule.sell_price_ = 15;

    game::ui::ShopSellEntryViewModel enabled{};
    game::ui::populateShopSellEntryViewModel(enabled, 1, 5, stack, &sell_rule, &item_catalog, &localization, true);
    EXPECT_EQ(enabled.index, 1);
    EXPECT_EQ(enabled.slot_index, 5);
    EXPECT_EQ(enabled.item_id_hash, entt::hashed_string{"potion"}.value());
    EXPECT_EQ(enabled.item_name, "Potion");
    EXPECT_EQ(enabled.count_text, "x3");
    EXPECT_EQ(enabled.price_text, "15 G");
    EXPECT_TRUE(enabled.is_selected);
    EXPECT_FALSE(enabled.is_disabled);

    game::ui::ShopSellEntryViewModel disabled{};
    game::ui::populateShopSellEntryViewModel(disabled, 0, 2, stack, nullptr, &item_catalog, &localization, false);
    EXPECT_EQ(disabled.slot_index, 2);
    EXPECT_EQ(disabled.price_text, "--");
    EXPECT_FALSE(disabled.is_selected);
    EXPECT_TRUE(disabled.is_disabled);
}

TEST(ShopMenuSellFlowSourceTest, ShopMenuSceneWiresSellPreviewModeSwitchAndExactSlotFlow) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/shop_menu_scene.h").lexically_normal();
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/shop_menu_scene.cpp").lexically_normal();
    const std::filesystem::path builder_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/shop_trade_list_builder.cpp").lexically_normal();
    const std::filesystem::path presenter_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/shop_menu_transaction_presenter.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    ASSERT_TRUE(std::filesystem::exists(builder_source_path)) << builder_source_path;
    ASSERT_TRUE(std::filesystem::exists(presenter_source_path)) << presenter_source_path;

    const std::string header = test_source_utils::readTextFile(header_path);
    const std::string source = test_source_utils::readTextFile(source_path);
    const std::string builder_source = test_source_utils::readTextFile(builder_source_path);
    const std::string presenter_source = test_source_utils::readTextFile(presenter_source_path);
    ASSERT_FALSE(header.empty());
    ASSERT_FALSE(source.empty());
    ASSERT_FALSE(builder_source.empty());
    ASSERT_FALSE(presenter_source.empty());

    const std::string init_ui_block = test_source_utils::extractFunctionBlock(source, "bool ShopMenuScene::initUI()");
    const std::string switch_mode_block =
        test_source_utils::extractFunctionBlock(source, "void ShopMenuScene::switchMode(const ShopMenuMode next_mode)");
    const std::string sell_preview_block =
        test_source_utils::extractFunctionBlock(source, "void ShopMenuScene::refreshSellPreview()");
    const std::string sell_select_block =
        test_source_utils::extractFunctionBlock(source, "void ShopMenuScene::selectSellEntry(const int index)");
    const std::string confirm_sell_block =
        test_source_utils::extractFunctionBlock(source, "void ShopMenuScene::confirmSell()");
    ASSERT_FALSE(init_ui_block.empty());
    ASSERT_FALSE(switch_mode_block.empty());
    ASSERT_FALSE(sell_preview_block.empty());
    ASSERT_FALSE(sell_select_block.empty());
    ASSERT_FALSE(confirm_sell_block.empty());

    EXPECT_NE(header.find("enum class ShopMenuMode"), std::string::npos);
    EXPECT_NE(header.find("ShopMenuCategory current_category_"), std::string::npos);
    EXPECT_NE(source.find("shop_transaction_service_->previewSell"), std::string::npos);
    EXPECT_NE(source.find("shop_transaction_service_->commitSell"), std::string::npos);
    EXPECT_NE(source.find("ShopTradeListBuilder::buildSellList"), std::string::npos);
    EXPECT_NE(builder_source.find("isItemInCategoryTab"), std::string::npos);
    EXPECT_NE(source.find("hasEntriesForCategory"), std::string::npos);
    EXPECT_NE(source.find("switchCategory"), std::string::npos);
    EXPECT_NE(source.find("\"has_equipment_entries\""), std::string::npos);
    EXPECT_NE(source.find("\"has_consumable_entries\""), std::string::npos);
    EXPECT_NE(source.find("current_mode_ == ShopMenuMode::Buy ? confirmBuy() : confirmSell()"), std::string::npos);
    EXPECT_NE(presenter_source.find("formatFailureText"), std::string::npos);
    EXPECT_NE(source.find("ShopTransactionPresenter::defaultEmptyText(localization_, toTradeMode(current_mode_), current_category_)"),
              std::string::npos);
    EXPECT_NE(source.find("detail_owned_label_"), std::string::npos);
    EXPECT_NE(source.find("primary_action_text_"), std::string::npos);
    EXPECT_NE(init_ui_block.find("\"sell_entry_select\""), std::string::npos);
    EXPECT_NE(init_ui_block.find("\"switch_mode_buy\""), std::string::npos);
    EXPECT_NE(init_ui_block.find("\"switch_mode_sell\""), std::string::npos);
    EXPECT_NE(init_ui_block.find("\"switch_category_consumable\""), std::string::npos);
    EXPECT_NE(init_ui_block.find("\"switch_category_equipment\""), std::string::npos);
    EXPECT_NE(init_ui_block.find("RegisterArray<decltype(sell_entries_)>()"), std::string::npos);
    EXPECT_NE(sell_preview_block.find("currentSellItemId()"), std::string::npos);
    EXPECT_NE(sell_preview_block.find("sell_entry->slot_index"), std::string::npos);
    EXPECT_NE(sell_select_block.find("document_controller_.markDirty(\"sell_entries\")"), std::string::npos);
    EXPECT_NE(confirm_sell_block.find("requested_sell_quantity_ = 1;"), std::string::npos);
    EXPECT_NE(confirm_sell_block.find("markTradeListsDirty();"), std::string::npos);
    EXPECT_NE(switch_mode_block.find("current_mode_ = next_mode;"), std::string::npos);
    EXPECT_NE(switch_mode_block.find("refreshAll();"), std::string::npos);
}

TEST(ShopMenuSellFlowSourceTest, ShopMenuRmlBindsModeToggleAndSellList) {
    const std::filesystem::path rml_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/shop_menu.rml").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(rml_path)) << rml_path;

    const std::string rml = test_source_utils::readTextFile(rml_path);
    ASSERT_FALSE(rml.empty());

    EXPECT_NE(rml.find("data-event-click=\"switch_mode_buy\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"switch_mode_sell\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"switch_category_consumable\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"switch_category_equipment\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-focused=\"is_category_tabs_focused\""), std::string::npos);
    EXPECT_NE(rml.find("<div class=\"shop-entry-row\" data-for=\"entry : sell_entries\">"), std::string::npos);
    EXPECT_NE(rml.find("data-for=\"entry : sell_entries\""), std::string::npos);
    EXPECT_NE(rml.find("id=\"shop-sell-entry-{{ entry.slot_index }}\""), std::string::npos);
    EXPECT_NE(rml.find("data-attr-data-shop-index=\"entry.index\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"sell_entry_select\""), std::string::npos);
    EXPECT_NE(rml.find("data-rml=\"entry.item_name\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-disabled=\"entry.is_disabled\""), std::string::npos);
    EXPECT_NE(rml.find("{{ detail_owned_label }}"), std::string::npos);
    EXPECT_NE(rml.find("{{ primary_action_text }}"), std::string::npos);
    EXPECT_NE(rml.find("data-if=\"is_sell_mode"), std::string::npos);
    EXPECT_EQ(rml.find("class=\"shop-entry shop-sell-entry\"\n                            data-for=\"entry : sell_entries\""), std::string::npos);
    EXPECT_EQ(rml.find("data-event-click=\"sell_entry_select(entry.index)\""), std::string::npos);
}

} // namespace
} // namespace game::scene
// NOLINTEND
