// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/data/item_catalog.h"
#include "game/ui/shop_menu_support.h"

#include <array>

namespace game::ui {
namespace {

TEST(ShopMenuCategoryFilterTest, EquipmentTabContainsOnlyEquipmentItems) {
    constexpr std::array kCategories{
        game::data::ItemCategory::Tool,
        game::data::ItemCategory::Crop,
        game::data::ItemCategory::Seed,
        game::data::ItemCategory::Material,
        game::data::ItemCategory::Consumable,
        game::data::ItemCategory::Equipment,
        game::data::ItemCategory::Unknown,
    };

    for (const auto category : kCategories) {
        const bool is_equipment = category == game::data::ItemCategory::Equipment;
        EXPECT_EQ(isItemInCategoryTab(ShopMenuCategory::Equipment, category), is_equipment);
        EXPECT_EQ(isItemInCategoryTab(ShopMenuCategory::Consumable, category), !is_equipment);
    }
}

TEST(ShopMenuCategoryFilterTest, DefaultCategoryIsConsumable) {
    EXPECT_EQ(resolveShopMenuCategoryDefault(), ShopMenuCategory::Consumable);
}

} // namespace
} // namespace game::ui
// NOLINTEND
