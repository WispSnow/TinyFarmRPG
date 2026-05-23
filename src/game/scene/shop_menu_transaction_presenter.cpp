#include "game/scene/shop_menu_transaction_presenter.h"

#include <algorithm>

namespace game::scene {

std::string ShopMenuTransactionPresenter::formatGoldLabel(const int gold) {
    return "Gold: " + std::to_string(gold);
}

std::string ShopMenuTransactionPresenter::formatGoldValue(const int gold) {
    return std::to_string(gold) + " G";
}

std::string ShopMenuTransactionPresenter::formatQuantityText(const int quantity) {
    return "x" + std::to_string(std::max(0, quantity));
}

std::string_view ShopMenuTransactionPresenter::defaultEmptyText(const ShopTradeMode mode,
                                                                const game::ui::ShopMenuCategory category) {
    if (mode == ShopTradeMode::Buy) {
        return category == game::ui::ShopMenuCategory::Equipment ? "No equipment for sale." : "No consumables available.";
    }

    return category == game::ui::ShopMenuCategory::Equipment ? "No equipment to sell." : "No items to sell.";
}

std::string ShopMenuTransactionPresenter::formatFocusStatus(const ShopTradeMode mode,
                                                            const game::ui::ShopMenuFocusArea focus_area,
                                                            const bool quantity_adjustable) {
    switch (focus_area) {
        case game::ui::ShopMenuFocusArea::ModeToggle:
            return "Left / Right switches Buy and Sell. Down enters category tabs.";
        case game::ui::ShopMenuFocusArea::CategoryTabs:
            return "Left / Right switches category. Down enters the list.";
        case game::ui::ShopMenuFocusArea::EntryList:
            return mode == ShopTradeMode::Buy ? "Up / Down selects. Right opens quantity. Confirm goes to Buy."
                                              : "Up / Down selects. Right opens quantity. Confirm goes to Sell.";
        case game::ui::ShopMenuFocusArea::Quantity:
            if (!quantity_adjustable) {
                return mode == ShopTradeMode::Buy ? "Quantity is fixed at x1. Down or Confirm goes to Buy."
                                                  : "Quantity is fixed at x1. Down or Confirm goes to Sell.";
            }
            return mode == ShopTradeMode::Buy ? "Left / Right changes quantity. Down or Confirm goes to Buy."
                                              : "Left / Right changes quantity. Down or Confirm goes to Sell.";
        case game::ui::ShopMenuFocusArea::PrimaryAction:
            return mode == ShopTradeMode::Buy ? "Confirm to buy. Left returns to the list."
                                              : "Confirm to sell. Left returns to the list.";
    }

    return mode == ShopTradeMode::Buy ? "Confirm to buy." : "Confirm to sell.";
}

std::string ShopMenuTransactionPresenter::formatFailureText(
    const ShopTradeMode mode,
    const game::domain::ShopTradeFailureReason failure_reason) {
    using game::domain::ShopTradeFailureReason;

    switch (failure_reason) {
        case ShopTradeFailureReason::None:
            break;
        case ShopTradeFailureReason::InsufficientGold:
            return "Not enough gold.";
        case ShopTradeFailureReason::InventoryFull:
            return mode == ShopTradeMode::Buy ? "Inventory is full." : "Action unavailable.";
        case ShopTradeFailureReason::InvalidQuantity:
            return "Invalid quantity.";
        case ShopTradeFailureReason::InvalidPlayer:
            return "Action unavailable.";
        case ShopTradeFailureReason::InvalidShop:
            return mode == ShopTradeMode::Buy ? "This shop is unavailable." : "Action unavailable.";
        case ShopTradeFailureReason::InvalidItem:
            return "Action unavailable.";
        case ShopTradeFailureReason::ItemNotSoldHere:
            return "This item cannot be purchased here.";
        case ShopTradeFailureReason::ItemNotSellable:
            return "This item cannot be sold.";
        case ShopTradeFailureReason::SlotMismatch:
            return "This slot changed.";
        case ShopTradeFailureReason::InsufficientItemCount:
            return "Not enough items in this slot.";
    }

    return mode == ShopTradeMode::Buy ? "Purchase failed." : "Sale failed.";
}

std::string ShopMenuTransactionPresenter::formatSuccessText(const ShopTradeMode mode,
                                                            const std::string_view item_name,
                                                            const int quantity) {
    std::string text = mode == ShopTradeMode::Buy ? "Purchased " : "Sold ";
    text.append(item_name);
    if (quantity > 1) {
        text.push_back(' ');
        text.push_back('x');
        text += std::to_string(quantity);
    }
    text.push_back('.');
    return text;
}

} // namespace game::scene
