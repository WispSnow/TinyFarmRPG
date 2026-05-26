// NOLINTBEGIN
#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/runtime/rpg_catalog_loader.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

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

[[nodiscard]] bool loadProjectRpgCatalog(RpgCatalog& catalog, const ItemCatalog& item_catalog, std::string& out_error) {
    const auto root = std::filesystem::path{PROJECT_SOURCE_DIR};
    return game::runtime::loadRpgCatalogFromManifest(
        catalog,
        game::runtime::RpgCatalogLoadOptions{
            .manifest_path = (root / "assets/data/rpg/manifest.json").string(),
            .root_path = (root / "assets/data/rpg").string(),
            .item_catalog = &item_catalog,
        },
        out_error);
}

TEST(ShopCatalogTest, LoadsProjectShopAssetAndValidatesReferences) {
    const auto root = std::filesystem::path{PROJECT_SOURCE_DIR};
    ShopCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile((root / "assets/data/shops.json").string()));
    EXPECT_EQ(catalog.schemaVersion(), 1U);

    const auto* shop = catalog.findShop("shop.village.general");
    ASSERT_NE(shop, nullptr);
    EXPECT_EQ(shop->title_, "shop.village.general.title");
    ASSERT_EQ(shop->buy_entries_.size(), 14U);
    EXPECT_EQ(shop->buy_entries_[0].item_id_, "potion");
    EXPECT_EQ(shop->buy_entries_[0].buy_price_, 30);

    const auto* sell_rule = catalog.findSellRule("material_timber");
    ASSERT_NE(sell_rule, nullptr);
    EXPECT_EQ(sell_rule->sell_price_, 5);

    ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));

    std::string error{};
    EXPECT_TRUE(catalog.validateReferences(&item_catalog, error)) << error;

    constexpr std::array equipment_ids{
        "equip_wooden_sword",
        "equip_wooden_staff",
        "equip_wooden_helmet",
        "equip_wooden_armor",
        "equip_wooden_boots",
        "equip_wooden_accessory",
        "equip_iron_sword",
        "equip_iron_staff",
        "equip_iron_helmet",
        "equip_iron_armor",
        "equip_iron_boots",
        "equip_iron_accessory",
    };

    for (const std::string_view equipment_id : equipment_ids) {
        const auto buy_entry_it = std::ranges::find_if(
            shop->buy_entries_,
            [equipment_id](const ShopBuyEntryData& entry) {
                return entry.item_id_ == equipment_id;
            });
        ASSERT_NE(buy_entry_it, shop->buy_entries_.end()) << equipment_id;

        const auto* equipment_sell_rule = catalog.findSellRule(equipment_id);
        ASSERT_NE(equipment_sell_rule, nullptr) << equipment_id;
        EXPECT_LT(equipment_sell_rule->sell_price_, buy_entry_it->buy_price_) << equipment_id;
    }
}

TEST(ShopCatalogTest, MerchantPresetsSellEveryWeapon) {
    const auto root = std::filesystem::path{PROJECT_SOURCE_DIR};
    ShopCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile((root / "assets/data/shops.json").string()));

    ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));

    RpgCatalog rpg_catalog;
    std::string error{};
    ASSERT_TRUE(loadProjectRpgCatalog(rpg_catalog, item_catalog, error)) << error;

    std::vector<std::string> weapon_item_ids{};
    for (const auto* equipment : rpg_catalog.listEquipment()) {
        if (equipment->slot_ == EquipmentSlotId::Weapon) {
            weapon_item_ids.push_back(equipment->item_id_);
        }
    }
    ASSERT_FALSE(weapon_item_ids.empty());

    constexpr std::array merchant_shop_ids{
        "shop.village.general",
        "shop.village.general.day",
        "shop.village.general.night",
        "shop.village.general.post_slime_cleanup",
    };

    for (const std::string_view shop_id : merchant_shop_ids) {
        const auto* shop = catalog.findShop(shop_id);
        ASSERT_NE(shop, nullptr) << shop_id;

        for (const auto& weapon_item_id : weapon_item_ids) {
            EXPECT_NE(catalog.findBuyEntry(shop_id, ShopCatalog::hashId(weapon_item_id)), nullptr)
                << shop_id << " missing " << weapon_item_id;
        }
    }
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
