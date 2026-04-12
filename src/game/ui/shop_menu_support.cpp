#include "game/ui/shop_menu_support.h"

#include "game/data/item_catalog.h"
#include "game/data/shop_data.h"
#include "game/ui/rml_item_icon_helpers.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/DataTypeRegister.h>

#include <algorithm>
#include <string>

namespace game::ui {

ShopMenuFocusArea resolvePreferredShopMenuFocus(const bool has_current_entries) {
    return has_current_entries ? ShopMenuFocusArea::EntryList : ShopMenuFocusArea::ModeToggle;
}

ShopMenuNavigationDecision resolveShopMenuNavigation(const ShopMenuNavigationState& state,
                                                     const ShopMenuNavigationInput input) {
    ShopMenuNavigationDecision decision{};
    decision.next_focus_area = state.focus_area;
    decision.next_is_buy_mode = state.is_buy_mode;

    const bool has_current_entries = state.hasCurrentEntries();
    switch (state.focus_area) {
        case ShopMenuFocusArea::ModeToggle:
            if (input == ShopMenuNavigationInput::Left && !state.is_buy_mode) {
                decision.switch_mode = true;
                decision.next_is_buy_mode = true;
            } else if (input == ShopMenuNavigationInput::Right && state.is_buy_mode) {
                decision.switch_mode = true;
                decision.next_is_buy_mode = false;
            } else if (input == ShopMenuNavigationInput::Down || input == ShopMenuNavigationInput::Confirm) {
                decision.next_focus_area = resolvePreferredShopMenuFocus(has_current_entries);
            }
            break;

        case ShopMenuFocusArea::EntryList:
            if (!has_current_entries) {
                if (input == ShopMenuNavigationInput::Left) {
                    decision.next_focus_area = ShopMenuFocusArea::ModeToggle;
                }
                break;
            }

            switch (input) {
                case ShopMenuNavigationInput::Up:
                    decision.entry_delta = -1;
                    break;
                case ShopMenuNavigationInput::Down:
                    decision.entry_delta = 1;
                    break;
                case ShopMenuNavigationInput::Left:
                    decision.next_focus_area = ShopMenuFocusArea::ModeToggle;
                    break;
                case ShopMenuNavigationInput::Right:
                    decision.next_focus_area = ShopMenuFocusArea::Quantity;
                    break;
                case ShopMenuNavigationInput::Confirm:
                    decision.next_focus_area = ShopMenuFocusArea::PrimaryAction;
                    break;
            }
            break;

        case ShopMenuFocusArea::Quantity:
            if (!has_current_entries) {
                if (input == ShopMenuNavigationInput::Up || input == ShopMenuNavigationInput::Down ||
                    input == ShopMenuNavigationInput::Confirm) {
                    decision.next_focus_area = resolvePreferredShopMenuFocus(false);
                }
                break;
            }

            switch (input) {
                case ShopMenuNavigationInput::Up:
                    decision.next_focus_area = ShopMenuFocusArea::EntryList;
                    break;
                case ShopMenuNavigationInput::Down:
                case ShopMenuNavigationInput::Confirm:
                    decision.next_focus_area = ShopMenuFocusArea::PrimaryAction;
                    break;
                case ShopMenuNavigationInput::Left:
                    if (state.quantity_adjustable) {
                        decision.quantity_delta = -1;
                    }
                    break;
                case ShopMenuNavigationInput::Right:
                    if (state.quantity_adjustable) {
                        decision.quantity_delta = 1;
                    }
                    break;
            }
            break;

        case ShopMenuFocusArea::PrimaryAction:
            if (!has_current_entries) {
                if (input == ShopMenuNavigationInput::Left || input == ShopMenuNavigationInput::Up) {
                    decision.next_focus_area = resolvePreferredShopMenuFocus(false);
                }
                break;
            }

            switch (input) {
                case ShopMenuNavigationInput::Up:
                    decision.next_focus_area = ShopMenuFocusArea::Quantity;
                    break;
                case ShopMenuNavigationInput::Left:
                    decision.next_focus_area = ShopMenuFocusArea::EntryList;
                    break;
                case ShopMenuNavigationInput::Confirm:
                    decision.confirm_trade = true;
                    break;
                case ShopMenuNavigationInput::Down:
                case ShopMenuNavigationInput::Right:
                    break;
            }
            break;
    }

    return decision;
}

bool registerShopBuyEntryViewModelType(Rml::DataModelConstructor& constructor) {
    if (auto handle = constructor.RegisterStruct<ShopBuyEntryViewModel>()) {
        handle.RegisterMember("index", &ShopBuyEntryViewModel::index);
        handle.RegisterMember("icon_decorator", &ShopBuyEntryViewModel::icon_decorator);
        handle.RegisterMember("item_name", &ShopBuyEntryViewModel::item_name);
        handle.RegisterMember("price_text", &ShopBuyEntryViewModel::price_text);
        handle.RegisterMember("owned_text", &ShopBuyEntryViewModel::owned_text);
        handle.RegisterMember("is_selected", &ShopBuyEntryViewModel::is_selected);
        handle.RegisterMember("is_disabled", &ShopBuyEntryViewModel::is_disabled);
        return true;
    }

    return false;
}

bool registerShopSellEntryViewModelType(Rml::DataModelConstructor& constructor) {
    if (auto handle = constructor.RegisterStruct<ShopSellEntryViewModel>()) {
        handle.RegisterMember("index", &ShopSellEntryViewModel::index);
        handle.RegisterMember("slot_index", &ShopSellEntryViewModel::slot_index);
        handle.RegisterMember("icon_decorator", &ShopSellEntryViewModel::icon_decorator);
        handle.RegisterMember("item_name", &ShopSellEntryViewModel::item_name);
        handle.RegisterMember("count_text", &ShopSellEntryViewModel::count_text);
        handle.RegisterMember("price_text", &ShopSellEntryViewModel::price_text);
        handle.RegisterMember("is_selected", &ShopSellEntryViewModel::is_selected);
        handle.RegisterMember("is_disabled", &ShopSellEntryViewModel::is_disabled);
        return true;
    }

    return false;
}

int countOwnedItems(const game::component::InventoryComponent& inventory, const entt::id_type item_id_hash) {
    if (item_id_hash == 0) {
        return 0;
    }

    int owned_count = 0;
    for (const auto& slot : inventory.slots_) {
        if (slot.item_id_ == item_id_hash && slot.count_ > 0) {
            owned_count += slot.count_;
        }
    }

    return owned_count;
}

int resolveBuyQuantityUiMax(const game::data::ItemData& item) {
    if (item.stack_limit_ <= 1) {
        return 1;
    }

    return std::max(1, std::min(item.stack_limit_, 99));
}

int resolveSellQuantityUiMax(const game::component::ItemStack& stack) {
    return std::max(1, stack.count_);
}

void populateShopBuyEntryViewModel(ShopBuyEntryViewModel& view_model,
                                   const int index,
                                   const game::data::ShopBuyEntryData& buy_entry,
                                   const game::data::ItemCatalog* item_catalog,
                                   const int owned_count,
                                   const bool is_selected,
                                   const bool is_disabled) {
    view_model.index = index;
    view_model.item_id_hash = buy_entry.item_id_hash_;
    view_model.icon_decorator = buildItemIconDecorator(item_catalog, buy_entry.item_id_hash_);
    view_model.price_text = std::to_string(buy_entry.buy_price_) + " G";
    view_model.owned_text = "Owned: " + std::to_string(std::max(0, owned_count));
    view_model.is_selected = is_selected;
    view_model.is_disabled = is_disabled;

    if (const auto* item = item_catalog ? item_catalog->findItem(buy_entry.item_id_hash_) : nullptr;
        item != nullptr && !item->display_name_.empty()) {
        view_model.item_name = item->display_name_;
    } else {
        view_model.item_name = buy_entry.item_id_;
    }
}

void populateShopSellEntryViewModel(ShopSellEntryViewModel& view_model,
                                    const int index,
                                    const int slot_index,
                                    const game::component::ItemStack& stack,
                                    const game::data::ShopSellRuleData* sell_rule,
                                    const game::data::ItemCatalog* item_catalog,
                                    const bool is_selected) {
    view_model.index = index;
    view_model.slot_index = slot_index;
    view_model.item_id_hash = stack.item_id_;
    view_model.icon_decorator = buildItemIconDecorator(item_catalog, stack.item_id_);
    view_model.count_text = "x" + std::to_string(std::max(0, stack.count_));
    view_model.is_selected = is_selected;
    view_model.is_disabled = sell_rule == nullptr;
    view_model.price_text = sell_rule ? std::to_string(sell_rule->sell_price_) + " G" : "--";

    if (const auto* item = item_catalog ? item_catalog->findItem(stack.item_id_) : nullptr; item != nullptr) {
        view_model.item_name = item->display_name_.empty() ? item->id_str_ : item->display_name_;
        return;
    }

    view_model.item_name = "Unknown item";
}

} // namespace game::ui
