#pragma once

#include "game/component/inventory_component.h"

#include <RmlUi/Core/Types.h>
#include <entt/core/fwd.hpp>

namespace Rml {
class DataModelConstructor;
}

namespace game::data {
class ItemCatalog;
struct ItemData;
struct ShopBuyEntryData;
struct ShopSellRuleData;
}

namespace game::ui {

/// @brief 商店 Buy 列表单条目的 UI 快照。
struct ShopBuyEntryViewModel {
    int index{0};
    entt::id_type item_id_hash{0};
    Rml::String icon_decorator{"none"};
    Rml::String item_name{};
    Rml::String price_text{};
    Rml::String owned_text{};
    bool is_selected{false};
    bool is_disabled{false};
};

/// @brief 将 ShopBuyEntryViewModel 字段注册到 RmlUi。
[[nodiscard]] bool registerShopBuyEntryViewModelType(Rml::DataModelConstructor& constructor);

/// @brief 商店 Sell 列表单条目的 UI 快照。
struct ShopSellEntryViewModel {
    int index{0};
    int slot_index{-1};
    entt::id_type item_id_hash{0};
    Rml::String icon_decorator{"none"};
    Rml::String item_name{};
    Rml::String count_text{};
    Rml::String price_text{};
    bool is_selected{false};
    bool is_disabled{false};
};

/// @brief 将 ShopSellEntryViewModel 字段注册到 RmlUi。
[[nodiscard]] bool registerShopSellEntryViewModelType(Rml::DataModelConstructor& constructor);

/// @brief 统计背包中指定物品的总持有数。
[[nodiscard]] int countOwnedItems(const game::component::InventoryComponent& inventory, entt::id_type item_id_hash);

/// @brief 解析 Buy UI 允许调整到的最大数量。
[[nodiscard]] int resolveBuyQuantityUiMax(const game::data::ItemData& item);

/// @brief 解析 Sell UI 允许调整到的最大数量。
[[nodiscard]] int resolveSellQuantityUiMax(const game::component::ItemStack& stack);

/// @brief 根据商店条目与玩家当前持有量填充 UI 快照。
void populateShopBuyEntryViewModel(ShopBuyEntryViewModel& view_model,
                                   int index,
                                   const game::data::ShopBuyEntryData& buy_entry,
                                   const game::data::ItemCatalog* item_catalog,
                                   int owned_count,
                                   bool is_selected,
                                   bool is_disabled);

/// @brief 根据真实背包槽位与 sell rule 填充 UI 快照。
void populateShopSellEntryViewModel(ShopSellEntryViewModel& view_model,
                                    int index,
                                    int slot_index,
                                    const game::component::ItemStack& stack,
                                    const game::data::ShopSellRuleData* sell_rule,
                                    const game::data::ItemCatalog* item_catalog,
                                    bool is_selected);

} // namespace game::ui
