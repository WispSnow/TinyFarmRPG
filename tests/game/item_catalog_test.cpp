// NOLINTBEGIN
#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"

#include <filesystem>
#include <string_view>

namespace game::data {
namespace {

[[nodiscard]] std::filesystem::path writeItemConfig(std::string_view prefix, std::string_view body) {
    const auto temp_root = game::test::createUniqueTempDir(prefix);
    const auto config_path = temp_root / "items.json";
    game::test::writeTextFile(config_path, body);
    return config_path;
}

TEST(ItemCatalogTest, LoadsBattleUseAndPreservesStringId) {
    const auto config_path = writeItemConfig(
        "item_catalog_battle_use",
        R"json({
  "items": [
    {
      "id": "item.potion",
      "display_name": "Potion",
      "category": "consumable",
      "battle_use": {
        "consume": 2,
        "scope": "one_ally",
        "effects": [
          { "type": "recover_hp", "amount": 50 }
        ]
      }
    },
    {
      "id": "strawberry_item",
      "display_name": "Strawberry",
      "category": "crop",
      "on_use": {
        "consume": 1,
        "effects": [
          { "type": "add_item", "id": "strawberry_seed", "count": 3 }
        ]
      }
    }
  ]
})json");

    ItemCatalog catalog;
    ASSERT_TRUE(catalog.loadItemConfig(config_path.string()));

    const auto* potion = catalog.findItem(RpgCatalog::hashId("item.potion"));
    ASSERT_NE(potion, nullptr);
    EXPECT_EQ(potion->id_str_, "item.potion");
    ASSERT_TRUE(potion->battle_use_.has_value());
    EXPECT_EQ(potion->battle_use_->consume, 2);
    EXPECT_EQ(potion->battle_use_->scope, Scope::OneAlly);
    ASSERT_EQ(potion->battle_use_->effects.size(), 1U);
    EXPECT_EQ(potion->battle_use_->effects[0].type, BattleItemEffectType::RecoverHp);
    EXPECT_EQ(potion->battle_use_->effects[0].amount, 50);

    const auto* strawberry = catalog.findItem(RpgCatalog::hashId("strawberry_item"));
    ASSERT_NE(strawberry, nullptr);
    EXPECT_TRUE(strawberry->on_use_.has_value());
    EXPECT_FALSE(strawberry->battle_use_.has_value());
}

TEST(ItemCatalogTest, RejectsInvalidBattleUse) {
    const auto config_path = writeItemConfig(
        "item_catalog_invalid_battle_use",
        R"json({
  "items": [
    {
      "id": "item.broken_potion",
      "display_name": "Broken Potion",
      "category": "consumable",
      "battle_use": {
        "consume": 1,
        "scope": "one_ally",
        "effects": [
          { "type": "recover_hp", "amount": 0 }
        ]
      }
    }
  ]
})json");

    ItemCatalog catalog;
    EXPECT_FALSE(catalog.loadItemConfig(config_path.string()));
}

} // namespace
} // namespace game::data
// NOLINTEND
