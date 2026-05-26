// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string_view>

#include <entt/core/hashed_string.hpp>

#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/data/shop_data.h"
#include "game/debug/shop_debug_panel_helpers.h"
#include "game/domain/shop_transaction_service.h"
#include "game/runtime/localization_service.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

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

} // namespace

namespace game::debug {
namespace {

TEST(ShopDebugPanelHelpersTest, BuildShopDebugBuyRowShowsOwnedCountAndPrice) {
    const auto item_catalog = loadProjectItemCatalog();
    const auto localization = loadEnglishLocalization();

    game::data::ShopBuyEntryData buy_entry{};
    buy_entry.item_id_ = "potion";
    buy_entry.item_id_hash_ = entt::hashed_string{"potion"}.value();
    buy_entry.buy_price_ = 30;

    const auto row = buildShopDebugBuyRow(buy_entry, &item_catalog, &localization, 7);

    EXPECT_EQ(row.item_id_hash, entt::hashed_string{"potion"}.value());
    EXPECT_EQ(row.item_id, "potion");
    EXPECT_EQ(row.item_name, "Potion");
    EXPECT_EQ(row.owned_count, 7);
    EXPECT_EQ(row.buy_price, 30);
}

TEST(ShopDebugPanelHelpersTest, BuildShopDebugSellRowPreservesSlotIndexAndSellableState) {
    const auto item_catalog = loadProjectItemCatalog();
    const auto localization = loadEnglishLocalization();

    game::component::ItemStack stack{};
    stack.item_id_ = entt::hashed_string{"potion"}.value();
    stack.count_ = 3;

    game::data::ShopSellRuleData sell_rule{};
    sell_rule.item_id_ = "potion";
    sell_rule.item_id_hash_ = stack.item_id_;
    sell_rule.sell_price_ = 15;

    const auto row = buildShopDebugSellRow(5, stack, &sell_rule, &item_catalog, &localization);

    EXPECT_EQ(row.slot_index, 5);
    EXPECT_EQ(row.item_id_hash, entt::hashed_string{"potion"}.value());
    EXPECT_EQ(row.item_id, "potion");
    EXPECT_EQ(row.item_name, "Potion");
    EXPECT_EQ(row.stack_count, 3);
    EXPECT_EQ(row.sell_price, 15);
    EXPECT_TRUE(row.is_sellable);
}

TEST(ShopDebugPanelHelpersTest, BuildShopDebugSellRowMarksUnsellableWhenRuleMissingOrMismatched) {
    const auto item_catalog = loadProjectItemCatalog();
    const auto localization = loadEnglishLocalization();

    game::component::ItemStack stack{};
    stack.item_id_ = entt::hashed_string{"material_timber"}.value();
    stack.count_ = 4;

    const auto missing_rule_row = buildShopDebugSellRow(2, stack, nullptr, &item_catalog, &localization);
    EXPECT_FALSE(missing_rule_row.is_sellable);
    EXPECT_EQ(missing_rule_row.sell_price, 0);

    game::data::ShopSellRuleData mismatched_rule{};
    mismatched_rule.item_id_ = "potion";
    mismatched_rule.item_id_hash_ = entt::hashed_string{"potion"}.value();
    mismatched_rule.sell_price_ = 15;

    EXPECT_FALSE(isShopDebugSellable(stack, &mismatched_rule));
    const auto mismatched_row = buildShopDebugSellRow(2, stack, &mismatched_rule, &item_catalog, &localization);
    EXPECT_FALSE(mismatched_row.is_sellable);
}

TEST(ShopDebugPanelHelpersTest, ClampShopDebugQuantityKeepsRangeAtLeastOne) {
    EXPECT_EQ(clampShopDebugQuantity(-5, 0), 1);
    EXPECT_EQ(clampShopDebugQuantity(0, 9), 1);
    EXPECT_EQ(clampShopDebugQuantity(3, 9), 3);
    EXPECT_EQ(clampShopDebugQuantity(99, 9), 9);
}

TEST(ShopDebugPanelHelpersTest, FailureLabelsCoverBuyAndSellModes) {
    using game::domain::ShopTradeFailureReason;

    EXPECT_EQ(shopDebugTradeFailureLabel(ShopDebugTradeMode::Buy, ShopTradeFailureReason::None), "Ready to buy.");
    EXPECT_EQ(shopDebugTradeFailureLabel(ShopDebugTradeMode::Sell, ShopTradeFailureReason::None), "Ready to sell.");
    EXPECT_EQ(
        shopDebugTradeFailureLabel(ShopDebugTradeMode::Buy, ShopTradeFailureReason::InsufficientGold),
        "Not enough gold.");
    EXPECT_EQ(
        shopDebugTradeFailureLabel(ShopDebugTradeMode::Sell, ShopTradeFailureReason::ItemNotSellable),
        "Item cannot be sold.");
    EXPECT_EQ(
        shopDebugTradeFailureLabel(ShopDebugTradeMode::Sell, ShopTradeFailureReason::SlotMismatch),
        "Selected slot no longer matches.");
}

} // namespace
} // namespace game::debug
// NOLINTEND
