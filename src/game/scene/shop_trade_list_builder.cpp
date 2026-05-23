#include "game/scene/shop_trade_list_builder.h"

#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/data/shop_data.h"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace {

[[nodiscard]] int clampSelectionIndex(const int index, const int size) {
    if (size <= 0) {
        return 0;
    }
    return std::clamp(index, 0, size - 1);
}

} // namespace

namespace game::scene {

ShopBuyListSnapshot ShopTradeListBuilder::buildBuyList(const game::data::ShopData* shop_data,
                                                       const game::data::ItemCatalog* item_catalog,
                                                       const game::component::InventoryComponent* inventory,
                                                       const game::ui::ShopMenuCategory category,
                                                       const int selected_index,
                                                       const std::string_view shop_id) {
    ShopBuyListSnapshot snapshot{};
    if (!shop_data || !item_catalog) {
        return snapshot;
    }

    snapshot.entry_refs.reserve(shop_data->buy_entries_.size());
    snapshot.entries.reserve(shop_data->buy_entries_.size());

    for (const auto& buy_entry : shop_data->buy_entries_) {
        const auto* item = item_catalog->findItem(buy_entry.item_id_hash_);
        if (!item) {
            spdlog::error("ShopMenuScene: shop_id='{}' 的 buy entry item_id='{}' 在 ItemCatalog 中缺失，已跳过。",
                          shop_id,
                          buy_entry.item_id_);
            continue;
        }
        if (!game::ui::isItemInCategoryTab(category, item->category_)) {
            continue;
        }

        const int owned_count = inventory ? game::ui::countOwnedItems(*inventory, buy_entry.item_id_hash_) : 0;
        game::ui::ShopBuyEntryViewModel view_model{};
        game::ui::populateShopBuyEntryViewModel(
            view_model,
            static_cast<int>(snapshot.entries.size()),
            buy_entry,
            item_catalog,
            owned_count,
            false,
            false);
        snapshot.entry_refs.push_back(&buy_entry);
        snapshot.entries.push_back(std::move(view_model));
    }

    snapshot.selected_index = clampSelectionIndex(selected_index, static_cast<int>(snapshot.entries.size()));
    for (auto& entry : snapshot.entries) {
        entry.is_selected = entry.index == snapshot.selected_index;
    }
    return snapshot;
}

ShopSellListSnapshot ShopTradeListBuilder::buildSellList(const game::component::InventoryComponent* inventory,
                                                         const game::data::ItemCatalog* item_catalog,
                                                         const game::data::ShopCatalog* shop_catalog,
                                                         const game::ui::ShopMenuCategory category,
                                                         const int selected_index) {
    ShopSellListSnapshot snapshot{};
    if (!inventory || !item_catalog || !shop_catalog) {
        return snapshot;
    }

    snapshot.entries.reserve(static_cast<std::size_t>(inventory->slotCount()));
    for (int slot_index = 0; slot_index < inventory->slotCount(); ++slot_index) {
        const auto& stack = inventory->slot(slot_index);
        if (stack.empty()) {
            continue;
        }

        const auto* item = item_catalog->findItem(stack.item_id_);
        const game::data::ItemCategory item_category = item ? item->category_ : game::data::ItemCategory::Unknown;
        if (!game::ui::isItemInCategoryTab(category, item_category)) {
            continue;
        }

        game::ui::ShopSellEntryViewModel view_model{};
        game::ui::populateShopSellEntryViewModel(
            view_model,
            static_cast<int>(snapshot.entries.size()),
            slot_index,
            stack,
            shop_catalog->findSellRule(stack.item_id_),
            item_catalog,
            false);
        snapshot.entries.push_back(std::move(view_model));
    }

    snapshot.selected_index = clampSelectionIndex(selected_index, static_cast<int>(snapshot.entries.size()));
    for (auto& entry : snapshot.entries) {
        entry.is_selected = entry.index == snapshot.selected_index;
    }
    return snapshot;
}

bool ShopTradeListBuilder::hasBuyEntriesForCategory(const game::data::ShopData* shop_data,
                                                    const game::data::ItemCatalog* item_catalog,
                                                    const game::ui::ShopMenuCategory category) {
    if (!shop_data || !item_catalog) {
        return false;
    }

    return std::ranges::any_of(shop_data->buy_entries_, [item_catalog, category](const auto& entry) {
        const auto* item = item_catalog->findItem(entry.item_id_hash_);
        return item != nullptr && game::ui::isItemInCategoryTab(category, item->category_);
    });
}

bool ShopTradeListBuilder::hasSellEntriesForCategory(const game::component::InventoryComponent* inventory,
                                                     const game::data::ItemCatalog* item_catalog,
                                                     const game::ui::ShopMenuCategory category) {
    if (!inventory || !item_catalog) {
        return false;
    }

    for (const auto& slot : inventory->slots_) {
        if (slot.empty()) {
            continue;
        }

        const auto* item = item_catalog->findItem(slot.item_id_);
        const game::data::ItemCategory item_category = item ? item->category_ : game::data::ItemCategory::Unknown;
        if (game::ui::isItemInCategoryTab(category, item_category)) {
            return true;
        }
    }

    return false;
}

} // namespace game::scene
