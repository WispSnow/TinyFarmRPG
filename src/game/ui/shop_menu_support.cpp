#include "game/ui/shop_menu_support.h"

#include "game/data/item_catalog.h"
#include "game/data/shop_data.h"
#include "game/ui/rml_item_icon_helpers.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/DataTypeRegister.h>

#include <algorithm>
#include <string>

namespace game::ui {

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

} // namespace game::ui
