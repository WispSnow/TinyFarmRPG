#pragma once

#include "game/ui/shop_menu_support.h"

#include <string_view>
#include <vector>

namespace game::component {
struct InventoryComponent;
}

namespace game::data {
class ItemCatalog;
class ShopCatalog;
struct ShopBuyEntryData;
struct ShopData;
}

namespace game::runtime {
class LocalizationService;
}

namespace game::scene {

/// @brief Built buy-list snapshot plus lookup pointers back to catalog entries.
struct ShopBuyListSnapshot {
    std::vector<const game::data::ShopBuyEntryData*> entry_refs{};
    std::vector<game::ui::ShopBuyEntryViewModel> entries{};
    int selected_index{0};
};

/// @brief Built sell-list snapshot for the active inventory category.
struct ShopSellListSnapshot {
    std::vector<game::ui::ShopSellEntryViewModel> entries{};
    int selected_index{0};
};

/// @brief Builds shop buy/sell list view models from catalog and inventory state.
class ShopTradeListBuilder final {
public:
    [[nodiscard]] static ShopBuyListSnapshot buildBuyList(const game::data::ShopData* shop_data,
                                                          const game::data::ItemCatalog* item_catalog,
                                                          const game::component::InventoryComponent* inventory,
                                                          const game::runtime::LocalizationService* localization,
                                                          game::ui::ShopMenuCategory category,
                                                          int selected_index,
                                                          std::string_view shop_id);

    [[nodiscard]] static ShopSellListSnapshot buildSellList(const game::component::InventoryComponent* inventory,
                                                            const game::data::ItemCatalog* item_catalog,
                                                            const game::data::ShopCatalog* shop_catalog,
                                                            const game::runtime::LocalizationService* localization,
                                                            game::ui::ShopMenuCategory category,
                                                            int selected_index);

    [[nodiscard]] static bool hasBuyEntriesForCategory(const game::data::ShopData* shop_data,
                                                       const game::data::ItemCatalog* item_catalog,
                                                       game::ui::ShopMenuCategory category);

    [[nodiscard]] static bool hasSellEntriesForCategory(const game::component::InventoryComponent* inventory,
                                                        const game::data::ItemCatalog* item_catalog,
                                                        game::ui::ShopMenuCategory category);
};

} // namespace game::scene
