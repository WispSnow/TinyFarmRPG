// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include <entt/core/hashed_string.hpp>

#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/data/shop_data.h"
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

    game::data::ShopBuyEntryData buy_entry{};
    buy_entry.item_id_ = "potion";
    buy_entry.item_id_hash_ = entt::hashed_string{"potion"}.value();
    buy_entry.buy_price_ = 30;

    game::ui::ShopBuyEntryViewModel view_model{};
    game::ui::populateShopBuyEntryViewModel(view_model, 2, buy_entry, &item_catalog, 7, true, false);

    EXPECT_EQ(view_model.index, 2);
    EXPECT_EQ(view_model.item_id_hash, entt::hashed_string{"potion"}.value());
    EXPECT_EQ(view_model.item_name, "Potion");
    EXPECT_EQ(view_model.price_text, "30 G");
    EXPECT_EQ(view_model.owned_text, "Owned: 7");
    EXPECT_TRUE(view_model.is_selected);
    EXPECT_FALSE(view_model.is_disabled);
}

TEST(ShopMenuBuyFlowSourceTest, ShopMenuSceneWiresPreviewCommitAndStatusRefresh) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/shop_menu_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(source.empty());
    const std::string select_block =
        test_source_utils::extractFunctionBlock(source, "void ShopMenuScene::selectBuyEntry(const int index)");
    const std::string destructor_block =
        test_source_utils::extractFunctionBlock(source, "ShopMenuScene::~ShopMenuScene()");
    ASSERT_FALSE(select_block.empty());
    ASSERT_FALSE(destructor_block.empty());

    EXPECT_NE(source.find("shop_transaction_service_->previewBuy"), std::string::npos);
    EXPECT_NE(source.find("shop_transaction_service_->commitBuy"), std::string::npos);
    EXPECT_NE(source.find("clearStatusOverride();"), std::string::npos);
    EXPECT_NE(source.find("formatFailureText"), std::string::npos);
    EXPECT_NE(source.find("formatSuccessText"), std::string::npos);
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

    EXPECT_NE(rml.find("data-for=\"entry : buy_entries\""), std::string::npos);
    EXPECT_NE(rml.find("data-class-selected=\"entry.is_selected\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"buy_entry_select(entry.index)\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"adjust_quantity(-1)\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"adjust_quantity(1)\""), std::string::npos);
    EXPECT_NE(rml.find("id=\"shop-primary-action-button\""), std::string::npos);
    EXPECT_NE(rml.find("data-event-click=\"confirm_trade\""), std::string::npos);
    EXPECT_NE(rml.find("{{ detail_after_gold_text }}"), std::string::npos);
    EXPECT_NE(rml.find("{{ status_text }}"), std::string::npos);

    EXPECT_NE(rcss.find(".shop-buy-entry.selected"), std::string::npos);
    EXPECT_NE(rcss.find("#shop-quantity-controls"), std::string::npos);
    EXPECT_NE(rcss.find(".shop-qty-button"), std::string::npos);
    EXPECT_NE(rcss.find(".shop-action-button"), std::string::npos);
}

} // namespace
} // namespace game::scene
// NOLINTEND
