#include "game/ui/inventory_menu_tab_shortcuts.h"

#include <entt/core/hashed_string.hpp>

#include <array>

using namespace entt::literals;

namespace game::ui {
namespace {

constexpr std::array INVENTORY_MENU_TAB_SHORTCUTS{
    InventoryMenuTabShortcut{"inventory", MenuTabId::Inventory},
    InventoryMenuTabShortcut{"inventory_tab_equipment", MenuTabId::Equipment},
    InventoryMenuTabShortcut{"inventory_tab_quests", MenuTabId::Quests},
    InventoryMenuTabShortcut{"inventory_tab_map", MenuTabId::Map},
    InventoryMenuTabShortcut{"inventory_tab_options", MenuTabId::Options},
};

} // namespace

std::span<const InventoryMenuTabShortcut> inventoryMenuTabShortcuts() {
    return INVENTORY_MENU_TAB_SHORTCUTS;
}

std::optional<MenuTabId> tabForInventoryMenuAction(const entt::id_type action_id) {
    switch (action_id) {
        case "inventory"_hs:
            return MenuTabId::Inventory;
        case "inventory_tab_equipment"_hs:
            return MenuTabId::Equipment;
        case "inventory_tab_quests"_hs:
            return MenuTabId::Quests;
        case "inventory_tab_map"_hs:
            return MenuTabId::Map;
        case "inventory_tab_options"_hs:
            return MenuTabId::Options;
        default:
            return std::nullopt;
    }
}

int tabsetIndexForMenuTab(const MenuTabId tab_id) {
    return static_cast<int>(tab_id);
}

} // namespace game::ui
