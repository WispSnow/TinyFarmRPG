// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <string_view>

#include <entt/core/hashed_string.hpp>

#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/data/shop_data.h"
#include "game/runtime/localization_service.h"
#include "game/scene/shop_menu_transaction_presenter.h"
#include "game/scene/shop_trade_list_builder.h"
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

[[nodiscard]] game::runtime::LocalizationService loadLocalization(const std::string_view language_tag) {
    game::runtime::LocalizationService localization;
    const auto manifest_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/i18n/languages.json").lexically_normal();
    EXPECT_TRUE(localization.loadLanguageIndex(manifest_path.string()));
    EXPECT_TRUE(localization.setLanguage(language_tag));
    return localization;
}

[[nodiscard]] game::runtime::LocalizationService loadEnglishLocalization() {
    return loadLocalization("en-US");
}

TEST(ShopMenuSupportTest, CountOwnedItemsAggregatesAcrossMatchingStacks) {
    game::component::InventoryComponent inventory;
    inventory.slot(0).item_id_ = entt::hashed_string{"potion"}.value();
    inventory.slot(0).count_ = 2;
    inventory.slot(1).item_id_ = entt::hashed_string{"strawberry_seed"}.value();
    inventory.slot(1).count_ = 4;
    inventory.slot(2).item_id_ = entt::hashed_string{"potion"}.value();
    inventory.slot(2).count_ = 3;

    EXPECT_EQ(game::ui::countOwnedItems(inventory, entt::hashed_string{"potion"}.value()), 5);
    EXPECT_EQ(game::ui::countOwnedItems(inventory, entt::hashed_string{"material_timber"}.value()), 0);
}

TEST(ShopMenuSupportTest, ResolveBuyQuantityUiMaxRespectsStackLimitAnd99Cap) {
    const auto item_catalog = loadProjectItemCatalog();
    const auto* potion = item_catalog.findItem(entt::hashed_string{"potion"}.value());
    const auto* strawberry_seed = item_catalog.findItem(entt::hashed_string{"strawberry_seed"}.value());
    ASSERT_NE(potion, nullptr);
    ASSERT_NE(strawberry_seed, nullptr);

    EXPECT_EQ(game::ui::resolveBuyQuantityUiMax(*potion), 99);
    EXPECT_EQ(game::ui::resolveBuyQuantityUiMax(*strawberry_seed), 99);

    game::data::ItemData tool_like{};
    tool_like.stack_limit_ = 1;
    EXPECT_EQ(game::ui::resolveBuyQuantityUiMax(tool_like), 1);

    game::data::ItemData capped{};
    capped.stack_limit_ = 12;
    EXPECT_EQ(game::ui::resolveBuyQuantityUiMax(capped), 12);
}

TEST(ShopMenuSupportTest, PopulateShopBuyEntryViewModelFormatsDisplayFields) {
    const auto item_catalog = loadProjectItemCatalog();
    const auto localization = loadEnglishLocalization();

    game::data::ShopBuyEntryData buy_entry{};
    buy_entry.item_id_ = "potion";
    buy_entry.item_id_hash_ = entt::hashed_string{"potion"}.value();
    buy_entry.buy_price_ = 30;

    game::ui::ShopBuyEntryViewModel view_model{};
    game::ui::populateShopBuyEntryViewModel(view_model, 2, buy_entry, &item_catalog, &localization, 7, true, false);

    EXPECT_EQ(view_model.index, 2);
    EXPECT_EQ(view_model.item_id_hash, entt::hashed_string{"potion"}.value());
    EXPECT_EQ(view_model.item_name, "Potion");
    EXPECT_EQ(view_model.price_text, "30 G");
    EXPECT_EQ(view_model.owned_text, "Owned: 7");
    EXPECT_TRUE(view_model.is_selected);
    EXPECT_FALSE(view_model.is_disabled);
}

TEST(ShopMenuSupportTest, TradeListBuilderFiltersBuyEntriesAndKeepsSelectionStable) {
    const auto item_catalog = loadProjectItemCatalog();
    const auto localization = loadEnglishLocalization();

    game::component::InventoryComponent inventory;
    inventory.slot(0).item_id_ = entt::hashed_string{"potion"}.value();
    inventory.slot(0).count_ = 3;

    game::data::ShopData shop_data{};
    shop_data.buy_entries_.push_back(game::data::ShopBuyEntryData{
        .item_id_ = "potion",
        .item_id_hash_ = entt::hashed_string{"potion"}.value(),
        .buy_price_ = 30});
    shop_data.buy_entries_.push_back(game::data::ShopBuyEntryData{
        .item_id_ = "equip_wooden_sword",
        .item_id_hash_ = entt::hashed_string{"equip_wooden_sword"}.value(),
        .buy_price_ = 120});

    const auto consumables = ShopTradeListBuilder::buildBuyList(
        &shop_data,
        &item_catalog,
        &inventory,
        &localization,
        game::ui::ShopMenuCategory::Consumable,
        5,
        "shop.test");
    ASSERT_EQ(consumables.entries.size(), 1U);
    EXPECT_EQ(consumables.selected_index, 0);
    EXPECT_EQ(consumables.entries.front().item_name, "Potion");
    EXPECT_EQ(consumables.entries.front().owned_text, "Owned: 3");
    EXPECT_TRUE(consumables.entries.front().is_selected);

    const auto equipment = ShopTradeListBuilder::buildBuyList(
        &shop_data,
        &item_catalog,
        nullptr,
        &localization,
        game::ui::ShopMenuCategory::Equipment,
        -3,
        "shop.test");
    ASSERT_EQ(equipment.entries.size(), 1U);
    EXPECT_EQ(equipment.entries.front().item_name, "Wooden Sword");
    EXPECT_TRUE(equipment.entries.front().is_selected);
}

TEST(ShopMenuSupportTest, TransactionPresenterFormatsCommonOutcomes) {
    EXPECT_EQ(ShopMenuTransactionPresenter::formatGoldLabel(nullptr, 42), "Gold: 42");
    EXPECT_EQ(ShopMenuTransactionPresenter::formatQuantityText(nullptr, 3), "x3");
    EXPECT_EQ(
        ShopMenuTransactionPresenter::formatFailureText(
            ShopTradeMode::Buy,
            game::domain::ShopTradeFailureReason::InsufficientGold,
            nullptr),
        "Not enough gold.");
    EXPECT_EQ(
        ShopMenuTransactionPresenter::formatSuccessText(ShopTradeMode::Sell, "Potion", 2, nullptr),
        "Sold Potion x2.");
}

TEST(ShopMenuSupportTest, TransactionPresenterFormatsLocalizedOutcomes) {
    const auto localization = loadLocalization("zh-Hans");

    EXPECT_EQ(ShopMenuTransactionPresenter::formatGoldLabel(&localization, 42), "金币：42");
    EXPECT_EQ(
        ShopMenuTransactionPresenter::formatFailureText(
            ShopTradeMode::Buy,
            game::domain::ShopTradeFailureReason::InsufficientGold,
            &localization),
        "金币不足。");
    EXPECT_EQ(
        ShopMenuTransactionPresenter::formatSuccessText(ShopTradeMode::Sell, "Potion", 2, &localization),
        "已出售 Potion x2。");
}

TEST(ShopMenuBuyFlowSourceTest, ShopMenuSceneWiresPreviewCommitAndStatusRefresh) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/shop_menu_scene.cpp").lexically_normal();
    const std::filesystem::path builder_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/shop_trade_list_builder.cpp").lexically_normal();
    const std::filesystem::path presenter_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/shop_menu_transaction_presenter.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    ASSERT_TRUE(std::filesystem::exists(builder_source_path)) << builder_source_path;
    ASSERT_TRUE(std::filesystem::exists(presenter_source_path)) << presenter_source_path;

    const std::string source = test_source_utils::readTextFile(source_path);
    const std::string builder_source = test_source_utils::readTextFile(builder_source_path);
    const std::string presenter_source = test_source_utils::readTextFile(presenter_source_path);
    ASSERT_FALSE(source.empty());
    ASSERT_FALSE(builder_source.empty());
    ASSERT_FALSE(presenter_source.empty());
    const std::string select_block =
        test_source_utils::extractFunctionBlock(source, "void ShopMenuScene::selectBuyEntry(const int index)");
    const std::string confirm_buy_block =
        test_source_utils::extractFunctionBlock(source, "void ShopMenuScene::confirmBuy()");
    const std::string destructor_block =
        test_source_utils::extractFunctionBlock(source, "ShopMenuScene::~ShopMenuScene()");
    ASSERT_FALSE(select_block.empty());
    ASSERT_FALSE(confirm_buy_block.empty());
    ASSERT_FALSE(destructor_block.empty());

    EXPECT_NE(source.find("shop_transaction_service_->previewBuy"), std::string::npos);
    EXPECT_NE(source.find("shop_transaction_service_->commitBuy"), std::string::npos);
    EXPECT_NE(source.find("ShopTradeListBuilder::buildBuyList"), std::string::npos);
    EXPECT_NE(builder_source.find("isItemInCategoryTab"), std::string::npos);
    EXPECT_NE(source.find("switch_category_consumable"), std::string::npos);
    EXPECT_NE(source.find("switch_category_equipment"), std::string::npos);
    EXPECT_NE(source.find("\"is_consumable_category\""), std::string::npos);
    EXPECT_NE(source.find("\"is_equipment_category\""), std::string::npos);
    EXPECT_NE(source.find("current_category_"), std::string::npos);
    EXPECT_NE(source.find("clearStatusOverride();"), std::string::npos);
    EXPECT_NE(source.find("ShopTransactionPresenter::formatFailureText"), std::string::npos);
    EXPECT_NE(source.find("ShopTransactionPresenter::formatSuccessText"), std::string::npos);
    EXPECT_NE(presenter_source.find("formatFailureText"), std::string::npos);
    EXPECT_NE(presenter_source.find("formatSuccessText"), std::string::npos);
    EXPECT_NE(source.find("\"detail_after_gold_text\""), std::string::npos);
    EXPECT_NE(source.find("\"detail_total_text\""), std::string::npos);
    EXPECT_NE(source.find("\"buy_enabled\""), std::string::npos);
    EXPECT_NE(source.find("\"quantity_decrease_enabled\""), std::string::npos);
    EXPECT_NE(source.find("\"quantity_increase_enabled\""), std::string::npos);
    EXPECT_NE(source.find("\"menu_confirm\"_hs"), std::string::npos);
    EXPECT_NE(source.find("\"menu_left\"_hs"), std::string::npos);
    EXPECT_NE(source.find("\"menu_right\"_hs"), std::string::npos);
    EXPECT_NE(select_block.find("document_controller_.markDirty(\"buy_entries\")"), std::string::npos);
    EXPECT_NE(select_block.find("buy_entries_[selected_buy_index_].is_selected = true;"), std::string::npos);
    EXPECT_EQ(select_block.find("refreshAll();"), std::string::npos);
    EXPECT_NE(confirm_buy_block.find("requested_buy_quantity_ = 1;"), std::string::npos);
    EXPECT_NE(destructor_block.find("if (context_pushed_)"), std::string::npos);
    EXPECT_NE(destructor_block.find("context_.getInputManager().popContext();"), std::string::npos);
}

TEST(ShopMenuBuyFlowSourceTest, ShopMenuRmlBindsBuyListAndQuantityControls) {
    const std::filesystem::path rml_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/shop_menu.rml").lexically_normal();
    const std::filesystem::path rcss_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/shop_menu.rcss").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(rml_path)) << rml_path;
    ASSERT_TRUE(std::filesystem::exists(rcss_path)) << rcss_path;

    const std::string rml = test_source_utils::readTextFile(rml_path);
    const std::string rcss = test_source_utils::readTextFile(rcss_path);
    ASSERT_FALSE(rml.empty());
    ASSERT_FALSE(rcss.empty());

    EXPECT_NE(rml.find("<div class=\"shop-entry-row\" data-for=\"entry : buy_entries\">"), std::string::npos);
    EXPECT_NE(rml.find("data-for=\"entry : buy_entries\""), std::string::npos);
    EXPECT_NE(rml.find("id=\"shop-category-tabs\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"switch_category_consumable\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"switch_category_equipment\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-selected=\"is_consumable_category\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-selected=\"is_equipment_category\""), std::string::npos);
    EXPECT_NE(rml.find("data-attr-data-shop-index=\"entry.index\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-selected=\"entry.is_selected\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"buy_entry_select\""), std::string::npos);
    EXPECT_NE(rml.find("data-rml=\"entry.item_name\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"adjust_quantity(-1)\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"adjust_quantity(1)\""), std::string::npos);
    EXPECT_NE(rml.find("id=\"shop-primary-action-button\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"confirm_trade\""), std::string::npos);
    EXPECT_NE(rml.find("{{ detail_after_gold_text }}"), std::string::npos);
    EXPECT_NE(rml.find("{{ status_text }}"), std::string::npos);
    EXPECT_EQ(rml.find("class=\"shop-entry shop-buy-entry\"\n                            data-for=\"entry : buy_entries\""), std::string::npos);
    EXPECT_EQ(rml.find("data-event-click=\"buy_entry_select(entry.index)\""), std::string::npos);

    EXPECT_NE(rcss.find(".shop-buy-entry.selected"), std::string::npos);
    EXPECT_NE(rcss.find(".shop-entry-row"), std::string::npos);
    EXPECT_NE(rcss.find("width: 172dp"), std::string::npos);
    EXPECT_NE(rcss.find("#shop-category-tabs"), std::string::npos);
    EXPECT_NE(rcss.find("overflow-x: hidden"), std::string::npos);
    EXPECT_NE(rcss.find("#shop-buy-list scrollbarvertical"), std::string::npos);
    EXPECT_NE(rcss.find("#shop-sell-list scrollbarvertical sliderbar"), std::string::npos);
    EXPECT_NE(rcss.find("#shop-quantity-controls"), std::string::npos);
    EXPECT_NE(rcss.find(".shop-qty-button"), std::string::npos);
    EXPECT_NE(rcss.find(".shop-action-button"), std::string::npos);
}

} // namespace
} // namespace game::scene
// NOLINTEND
