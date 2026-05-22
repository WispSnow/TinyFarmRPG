#include <gtest/gtest.h>

#include "game/ui/inventory_menu_tab_shortcuts.h"

#include <entt/core/hashed_string.hpp>

using namespace entt::literals;

namespace game::ui {
namespace {

TEST(InventoryMenuTabShortcutsTest, MapsActionsToTabs) {
    EXPECT_EQ(tabForInventoryMenuAction("inventory"_hs), MenuTabId::Inventory);
    EXPECT_EQ(tabForInventoryMenuAction("inventory_tab_equipment"_hs), MenuTabId::Equipment);
    EXPECT_EQ(tabForInventoryMenuAction("inventory_tab_quests"_hs), MenuTabId::Quests);
    EXPECT_EQ(tabForInventoryMenuAction("inventory_tab_map"_hs), MenuTabId::Map);
    EXPECT_EQ(tabForInventoryMenuAction("inventory_tab_options"_hs), MenuTabId::Options);
    EXPECT_FALSE(tabForInventoryMenuAction("hotbar"_hs).has_value());
}

TEST(InventoryMenuTabShortcutsTest, TabsetIndicesFollowMenuTabEnumOrder) {
    EXPECT_EQ(tabsetIndexForMenuTab(MenuTabId::Inventory), 0);
    EXPECT_EQ(tabsetIndexForMenuTab(MenuTabId::Equipment), 1);
    EXPECT_EQ(tabsetIndexForMenuTab(MenuTabId::Quests), 2);
    EXPECT_EQ(tabsetIndexForMenuTab(MenuTabId::Map), 3);
    EXPECT_EQ(tabsetIndexForMenuTab(MenuTabId::Options), 4);
}

TEST(InventoryMenuTabShortcutsTest, ShortcutListContainsEveryInventoryMenuTab) {
    const auto shortcuts = inventoryMenuTabShortcuts();

    ASSERT_EQ(shortcuts.size(), 5U);
    EXPECT_EQ(shortcuts[0].action_name, "inventory");
    EXPECT_EQ(shortcuts[0].tab_id, MenuTabId::Inventory);
    EXPECT_EQ(shortcuts[1].action_name, "inventory_tab_equipment");
    EXPECT_EQ(shortcuts[1].tab_id, MenuTabId::Equipment);
    EXPECT_EQ(shortcuts[2].action_name, "inventory_tab_quests");
    EXPECT_EQ(shortcuts[2].tab_id, MenuTabId::Quests);
    EXPECT_EQ(shortcuts[3].action_name, "inventory_tab_map");
    EXPECT_EQ(shortcuts[3].tab_id, MenuTabId::Map);
    EXPECT_EQ(shortcuts[4].action_name, "inventory_tab_options");
    EXPECT_EQ(shortcuts[4].tab_id, MenuTabId::Options);
}

} // namespace
} // namespace game::ui
