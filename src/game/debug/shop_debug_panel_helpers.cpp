#include "game/debug/shop_debug_panel_helpers.h"

#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/data/shop_data.h"
#include "game/domain/shop_transaction_service.h"

#include <algorithm>
#include <string_view>

namespace {

[[nodiscard]] std::string resolveDisplayName(const game::data::ItemCatalog* item_catalog,
                                             const entt::id_type item_id_hash,
                                             const std::string_view fallback_id) {
    if (item_catalog != nullptr) {
        if (const auto* item = item_catalog->findItem(item_id_hash)) {
            if (!item->display_name_.empty()) {
                return item->display_name_;
            }
            if (!item->id_str_.empty()) {
                return item->id_str_;
            }
        }
    }

    if (!fallback_id.empty()) {
        return std::string{fallback_id};
    }
    return "<missing item>";
}

[[nodiscard]] std::string resolveItemId(const game::data::ItemCatalog* item_catalog,
                                        const entt::id_type item_id_hash,
                                        const std::string_view fallback_id) {
    if (item_catalog != nullptr) {
        if (const auto* item = item_catalog->findItem(item_id_hash)) {
            if (!item->id_str_.empty()) {
                return item->id_str_;
            }
        }
    }

    if (!fallback_id.empty()) {
        return std::string{fallback_id};
    }

    return "<missing item>";
}

} // namespace

namespace game::debug {

ShopDebugBuyRow buildShopDebugBuyRow(const game::data::ShopBuyEntryData& buy_entry,
                                     const game::data::ItemCatalog* item_catalog,
                                     const int owned_count) {
    ShopDebugBuyRow row{};
    row.item_id_hash = buy_entry.item_id_hash_;
    row.item_id = buy_entry.item_id_;
    row.item_name = resolveDisplayName(item_catalog, buy_entry.item_id_hash_, buy_entry.item_id_);
    row.owned_count = std::max(0, owned_count);
    row.buy_price = std::max(0, buy_entry.buy_price_);
    return row;
}

ShopDebugSellRow buildShopDebugSellRow(const int slot_index,
                                       const game::component::ItemStack& stack,
                                       const game::data::ShopSellRuleData* sell_rule,
                                       const game::data::ItemCatalog* item_catalog) {
    ShopDebugSellRow row{};
    row.slot_index = slot_index;
    row.item_id_hash = stack.item_id_;
    row.item_id = resolveItemId(item_catalog, stack.item_id_, sell_rule != nullptr ? sell_rule->item_id_ : std::string_view{});
    row.item_name = resolveDisplayName(item_catalog, stack.item_id_, row.item_id);
    row.stack_count = std::max(0, stack.count_);
    row.sell_price = (sell_rule != nullptr) ? std::max(0, sell_rule->sell_price_) : 0;
    row.is_sellable = isShopDebugSellable(stack, sell_rule);
    return row;
}

bool isShopDebugSellable(const game::component::ItemStack& stack, const game::data::ShopSellRuleData* sell_rule) {
    return !stack.empty() && stack.item_id_ != entt::null && sell_rule != nullptr && sell_rule->item_id_hash_ == stack.item_id_ &&
           sell_rule->sell_price_ > 0;
}

int clampShopDebugQuantity(const int requested_quantity, const int max_quantity) {
    return std::clamp(requested_quantity, 1, std::max(1, max_quantity));
}

std::string_view shopDebugTradeFailureLabel(const ShopDebugTradeMode mode,
                                            const game::domain::ShopTradeFailureReason reason) {
    using game::domain::ShopTradeFailureReason;

    switch (reason) {
        case ShopTradeFailureReason::None: return mode == ShopDebugTradeMode::Buy ? "Ready to buy." : "Ready to sell.";
        case ShopTradeFailureReason::InsufficientGold: return "Not enough gold.";
        case ShopTradeFailureReason::InventoryFull: return "Inventory is full.";
        case ShopTradeFailureReason::ItemNotSoldHere: return "Item is not sold here.";
        case ShopTradeFailureReason::ItemNotSellable: return "Item cannot be sold.";
        case ShopTradeFailureReason::SlotMismatch: return "Selected slot no longer matches.";
        case ShopTradeFailureReason::InsufficientItemCount:
            return mode == ShopDebugTradeMode::Sell ? "Not enough items in the selected slot." : "Requested quantity is unavailable.";
        case ShopTradeFailureReason::InvalidPlayer:
        case ShopTradeFailureReason::InvalidShop:
        case ShopTradeFailureReason::InvalidItem:
        case ShopTradeFailureReason::InvalidQuantity:
        default: return mode == ShopDebugTradeMode::Buy ? "Buy action unavailable." : "Sell action unavailable.";
    }
}

} // namespace game::debug
