#include "game/scene/shop_menu_transaction_presenter.h"

#include "game/ui/localized_text.h"

#include <algorithm>
#include <utility>

namespace game::scene {

namespace {

[[nodiscard]] std::string formatShopText(const game::runtime::LocalizationService* localization,
                                         const std::string_view key,
                                         const game::ui::LocalizedFormatArgs& args,
                                         std::string fallback) {
    return game::ui::formatTextOrFallback(
        localization,
        key,
        args,
        [fallback = std::move(fallback)] { return fallback; });
}

} // namespace

std::string ShopMenuTransactionPresenter::formatGoldLabel(const game::runtime::LocalizationService* localization,
                                                          const int gold) {
    return formatShopText(
        localization,
        "shop.gold_label",
        {{"amount", std::to_string(gold)}},
        "Gold: " + std::to_string(gold));
}

std::string ShopMenuTransactionPresenter::formatGoldValue(const game::runtime::LocalizationService* localization,
                                                          const int gold) {
    return formatShopText(
        localization,
        "shop.gold_value",
        {{"amount", std::to_string(gold)}},
        std::to_string(gold) + " G");
}

std::string ShopMenuTransactionPresenter::formatQuantityText(const game::runtime::LocalizationService* localization,
                                                             const int quantity) {
    const int clamped_quantity = std::max(0, quantity);
    return formatShopText(
        localization,
        "shop.quantity",
        {{"quantity", std::to_string(clamped_quantity)}},
        "x" + std::to_string(clamped_quantity));
}

std::string ShopMenuTransactionPresenter::defaultEmptyText(const game::runtime::LocalizationService* localization,
                                                           const ShopTradeMode mode,
                                                           const game::ui::ShopMenuCategory category) {
    if (mode == ShopTradeMode::Buy) {
        return category == game::ui::ShopMenuCategory::Equipment
            ? game::ui::localizeTextOrFallback(localization, "shop.empty.buy_equipment", "No equipment for sale.")
            : game::ui::localizeTextOrFallback(localization, "shop.empty.buy_consumable", "No consumables available.");
    }

    return category == game::ui::ShopMenuCategory::Equipment
        ? game::ui::localizeTextOrFallback(localization, "shop.empty.sell_equipment", "No equipment to sell.")
        : game::ui::localizeTextOrFallback(localization, "shop.empty.sell_consumable", "No items to sell.");
}

std::string ShopMenuTransactionPresenter::formatOwnedLabel(const game::runtime::LocalizationService* localization,
                                                           const ShopTradeMode mode) {
    return mode == ShopTradeMode::Buy
        ? game::ui::localizeTextOrFallback(localization, "shop.detail.owned", "Owned")
        : game::ui::localizeTextOrFallback(localization, "shop.detail.in_slot", "In Slot");
}

std::string ShopMenuTransactionPresenter::formatFocusStatus(const ShopTradeMode mode,
                                                            const game::ui::ShopMenuFocusArea focus_area,
                                                            const bool quantity_adjustable,
                                                            const game::runtime::LocalizationService* localization) {
    switch (focus_area) {
        case game::ui::ShopMenuFocusArea::ModeToggle:
            return game::ui::localizeTextOrFallback(
                localization,
                "shop.status.focus.mode_toggle",
                "Left / Right switches Buy and Sell. Down enters category tabs.");
        case game::ui::ShopMenuFocusArea::CategoryTabs:
            return game::ui::localizeTextOrFallback(
                localization,
                "shop.status.focus.category_tabs",
                "Left / Right switches category. Down enters the list.");
        case game::ui::ShopMenuFocusArea::EntryList:
            return mode == ShopTradeMode::Buy
                ? game::ui::localizeTextOrFallback(
                      localization,
                      "shop.status.focus.entry_list_buy",
                      "Up / Down selects. Right opens quantity. Confirm goes to Buy.")
                : game::ui::localizeTextOrFallback(
                      localization,
                      "shop.status.focus.entry_list_sell",
                      "Up / Down selects. Right opens quantity. Confirm goes to Sell.");
        case game::ui::ShopMenuFocusArea::Quantity:
            if (!quantity_adjustable) {
                return mode == ShopTradeMode::Buy
                    ? game::ui::localizeTextOrFallback(
                          localization,
                          "shop.status.focus.quantity_fixed_buy",
                          "Quantity is fixed at x1. Down or Confirm goes to Buy.")
                    : game::ui::localizeTextOrFallback(
                          localization,
                          "shop.status.focus.quantity_fixed_sell",
                          "Quantity is fixed at x1. Down or Confirm goes to Sell.");
            }
            return mode == ShopTradeMode::Buy
                ? game::ui::localizeTextOrFallback(
                      localization,
                      "shop.status.focus.quantity_buy",
                      "Left / Right changes quantity. Down or Confirm goes to Buy.")
                : game::ui::localizeTextOrFallback(
                      localization,
                      "shop.status.focus.quantity_sell",
                      "Left / Right changes quantity. Down or Confirm goes to Sell.");
        case game::ui::ShopMenuFocusArea::PrimaryAction:
            return mode == ShopTradeMode::Buy
                ? game::ui::localizeTextOrFallback(
                      localization,
                      "shop.status.focus.primary_buy",
                      "Confirm to buy. Left returns to the list.")
                : game::ui::localizeTextOrFallback(
                      localization,
                      "shop.status.focus.primary_sell",
                      "Confirm to sell. Left returns to the list.");
    }

    return mode == ShopTradeMode::Buy
        ? game::ui::localizeTextOrFallback(localization, "shop.status.focus.buy", "Confirm to buy.")
        : game::ui::localizeTextOrFallback(localization, "shop.status.focus.sell", "Confirm to sell.");
}

std::string ShopMenuTransactionPresenter::formatFailureText(
    const ShopTradeMode mode,
    const game::domain::ShopTradeFailureReason failure_reason,
    const game::runtime::LocalizationService* localization) {
    using game::domain::ShopTradeFailureReason;

    switch (failure_reason) {
        case ShopTradeFailureReason::None:
            break;
        case ShopTradeFailureReason::InsufficientGold:
            return game::ui::localizeTextOrFallback(localization, "shop.failure.insufficient_gold", "Not enough gold.");
        case ShopTradeFailureReason::InventoryFull:
            return mode == ShopTradeMode::Buy
                ? game::ui::localizeTextOrFallback(localization, "shop.failure.inventory_full", "Inventory is full.")
                : game::ui::localizeTextOrFallback(
                      localization,
                      "shop.failure.action_unavailable",
                      "Action unavailable.");
        case ShopTradeFailureReason::InvalidQuantity:
            return game::ui::localizeTextOrFallback(localization, "shop.failure.invalid_quantity", "Invalid quantity.");
        case ShopTradeFailureReason::InvalidPlayer:
            return game::ui::localizeTextOrFallback(
                localization,
                "shop.failure.action_unavailable",
                "Action unavailable.");
        case ShopTradeFailureReason::InvalidShop:
            return mode == ShopTradeMode::Buy
                ? game::ui::localizeTextOrFallback(
                      localization,
                      "shop.failure.shop_unavailable",
                      "This shop is unavailable.")
                : game::ui::localizeTextOrFallback(
                      localization,
                      "shop.failure.action_unavailable",
                      "Action unavailable.");
        case ShopTradeFailureReason::InvalidItem:
            return game::ui::localizeTextOrFallback(
                localization,
                "shop.failure.action_unavailable",
                "Action unavailable.");
        case ShopTradeFailureReason::ItemNotSoldHere:
            return game::ui::localizeTextOrFallback(
                localization,
                "shop.failure.item_not_sold_here",
                "This item cannot be purchased here.");
        case ShopTradeFailureReason::ItemNotSellable:
            return game::ui::localizeTextOrFallback(
                localization,
                "shop.failure.item_not_sellable",
                "This item cannot be sold.");
        case ShopTradeFailureReason::SlotMismatch:
            return game::ui::localizeTextOrFallback(localization, "shop.failure.slot_mismatch", "This slot changed.");
        case ShopTradeFailureReason::InsufficientItemCount:
            return game::ui::localizeTextOrFallback(
                localization,
                "shop.failure.insufficient_item_count",
                "Not enough items in this slot.");
    }

    return mode == ShopTradeMode::Buy
        ? game::ui::localizeTextOrFallback(localization, "shop.failure.purchase_failed", "Purchase failed.")
        : game::ui::localizeTextOrFallback(localization, "shop.failure.sale_failed", "Sale failed.");
}

std::string ShopMenuTransactionPresenter::formatNoItemSelected(const game::runtime::LocalizationService* localization) {
    return game::ui::localizeTextOrFallback(localization, "shop.failure.no_item_selected", "No item selected.");
}

std::string ShopMenuTransactionPresenter::formatSuccessText(const ShopTradeMode mode,
                                                            const std::string_view item_name,
                                                            const int quantity,
                                                            const game::runtime::LocalizationService* localization) {
    const bool stack_text = quantity > 1;
    if (mode == ShopTradeMode::Buy) {
        if (stack_text) {
            return formatShopText(
                localization,
                "shop.success.buy_stack",
                {{"item", std::string{item_name}}, {"quantity", std::to_string(quantity)}},
                "Purchased " + std::string{item_name} + " x" + std::to_string(quantity) + ".");
        }
        return formatShopText(
            localization,
            "shop.success.buy",
            {{"item", std::string{item_name}}},
            "Purchased " + std::string{item_name} + ".");
    }

    if (stack_text) {
        return formatShopText(
            localization,
            "shop.success.sell_stack",
            {{"item", std::string{item_name}}, {"quantity", std::to_string(quantity)}},
            "Sold " + std::string{item_name} + " x" + std::to_string(quantity) + ".");
    }
    return formatShopText(
        localization,
        "shop.success.sell",
        {{"item", std::string{item_name}}},
        "Sold " + std::string{item_name} + ".");
}

} // namespace game::scene
