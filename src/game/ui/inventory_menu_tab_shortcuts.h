#pragma once

#include "game/ui/menu_tab_content.h"

#include <entt/core/fwd.hpp>

#include <optional>
#include <span>
#include <string_view>

namespace game::ui {

struct InventoryMenuTabShortcut {
    std::string_view action_name{};
    MenuTabId tab_id{MenuTabId::Inventory};
};

[[nodiscard]] std::span<const InventoryMenuTabShortcut> inventoryMenuTabShortcuts();
[[nodiscard]] std::optional<MenuTabId> tabForInventoryMenuAction(entt::id_type action_id);
[[nodiscard]] int tabsetIndexForMenuTab(MenuTabId tab_id);

} // namespace game::ui
