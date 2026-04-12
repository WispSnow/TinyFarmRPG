// NOLINTBEGIN
#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "game/data/item_catalog.h"
#include "game/data/shop_catalog.h"

#include <filesystem>
#include <string_view>

namespace game::data {
namespace {

[[nodiscard]] std::filesystem::path writeShopConfig(std::string_view prefix, std::string_view body) {
    const auto temp_root = game::test::createUniqueTempDir(prefix);
    const auto config_path = temp_root / "shops.json";
    game::test::writeTextFile(config_path, body);
    return config_path;
}

[[nodiscard]] bool loadProjectItemCatalog(ItemCatalog& catalog) {
    const auto root = std::filesystem::path{PROJECT_SOURCE_DIR};
    return catalog.loadItemConfig((root / "assets/data/item_config.json").string());
}

TEST(ShopCatalogTest, LoadsProjectShopAssetAndValidatesReferences) {
    const auto root = std::filesystem::path{PROJECT_SOURCE_DIR};
    ShopCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile((root / "assets/data/shops.json").string()));
    EXPECT_EQ(catalog.schemaVersion(), 1U);

    const auto* shop = catalog.findShop("shop.village.general");
    ASSERT_NE(shop, nullptr);
    EXPECT_EQ(shop->title_, "Village General Store");
    ASSERT_EQ(shop->buy_entries_.size(), 2U);
    EXPECT_EQ(shop->buy_entries_[0].item_id_, "potion");
    EXPECT_EQ(shop->buy_entries_[0].buy_price_, 30);

    const auto* sell_rule = catalog.findSellRule("material_timber");
    ASSERT_NE(sell_rule, nullptr);
    EXPECT_EQ(sell_rule->sell_price_, 5);

    ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));

    std::string error{};
    EXPECT_TRUE(catalog.validateReferences(&item_catalog, error)) << error;
}

TEST(ShopCatalogTest, RejectsDuplicateShopId) {
    const auto path = writeShopConfig(
        "shop_catalog_duplicate_shop_id",
        R"json({
  "schema_version": 1,
  "shops": [
    {
      "id": "shop.alpha",
      "title": "Alpha",
      "buy_entries": [
        { "item_id": "potion", "buy_price": 10 }
      ]
    },
    {
      "id": "shop.alpha",
      "title": "Beta",
      "buy_entries": [
        { "item_id": "strawberry_seed", "buy_price": 8 }
      ]
    }
  ]
})json");

    ShopCatalog catalog;
    EXPECT_FALSE(catalog.loadFromFile(path.string()));
}

TEST(ShopCatalogTest, RejectsDuplicateBuyEntryWithinShop) {
    const auto path = writeShopConfig(
        "shop_catalog_duplicate_buy_entry",
        R"json({
  "schema_version": 1,
  "shops": [
    {
      "id": "shop.alpha",
      "title": "Alpha",
      "buy_entries": [
        { "item_id": "potion", "buy_price": 10 },
        { "item_id": "potion", "buy_price": 12 }
      ]
    }
  ]
})json");

    ShopCatalog catalog;
    EXPECT_FALSE(catalog.loadFromFile(path.string()));
}

TEST(ShopCatalogTest, RejectsInvalidPricesAndMissingTitle) {
    const auto invalid_price_path = writeShopConfig(
        "shop_catalog_invalid_price",
        R"json({
  "schema_version": 1,
  "shops": [
    {
      "id": "shop.alpha",
      "title": "Alpha",
      "buy_entries": [
        { "item_id": "potion", "buy_price": 0 }
      ]
    }
  ]
})json");

    ShopCatalog invalid_price_catalog;
    EXPECT_FALSE(invalid_price_catalog.loadFromFile(invalid_price_path.string()));

    const auto missing_title_path = writeShopConfig(
        "shop_catalog_missing_title",
        R"json({
  "schema_version": 1,
  "shops": [
    {
      "id": "shop.alpha",
      "buy_entries": [
        { "item_id": "potion", "buy_price": 10 }
      ]
    }
  ]
})json");

    ShopCatalog missing_title_catalog;
    EXPECT_FALSE(missing_title_catalog.loadFromFile(missing_title_path.string()));
}

TEST(ShopCatalogTest, ValidateReferencesFailsWhenItemMissing) {
    const auto path = writeShopConfig(
        "shop_catalog_missing_item_reference",
        R"json({
  "schema_version": 1,
  "shops": [
    {
      "id": "shop.alpha",
      "title": "Alpha",
      "buy_entries": [
        { "item_id": "item.missing", "buy_price": 10 }
      ]
    }
  ],
  "sell_rules": [
    { "item_id": "item.missing.sell", "sell_price": 5 }
  ]
})json");

    ShopCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(path.string()));

    ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));

    std::string error{};
    EXPECT_FALSE(catalog.validateReferences(&item_catalog, error));
    EXPECT_FALSE(error.empty());
}

TEST(ShopCatalogTest, AllowsSellPriceAboveLowestBuyPriceWithWarningOnly) {
    const auto path = writeShopConfig(
        "shop_catalog_sell_above_buy_warning",
        R"json({
  "schema_version": 1,
  "shops": [
    {
      "id": "shop.alpha",
      "title": "Alpha",
      "buy_entries": [
        { "item_id": "potion", "buy_price": 10 }
      ]
    }
  ],
  "sell_rules": [
    { "item_id": "potion", "sell_price": 20 }
  ]
})json");

    ShopCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(path.string()));

    ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));

    std::string error{};
    EXPECT_TRUE(catalog.validateReferences(&item_catalog, error)) << error;
}

} // namespace
} // namespace game::data
// NOLINTEND
