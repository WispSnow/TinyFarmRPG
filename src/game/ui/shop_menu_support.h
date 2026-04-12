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

/// @brief 统计背包中指定物品的总持有数。
[[nodiscard]] int countOwnedItems(const game::component::InventoryComponent& inventory, entt::id_type item_id_hash);

/// @brief 解析 Buy UI 允许调整到的最大数量。
[[nodiscard]] int resolveBuyQuantityUiMax(const game::data::ItemData& item);

/// @brief 根据商店条目与玩家当前持有量填充 UI 快照。
void populateShopBuyEntryViewModel(ShopBuyEntryViewModel& view_model,
                                   int index,
                                   const game::data::ShopBuyEntryData& buy_entry,
                                   const game::data::ItemCatalog* item_catalog,
                                   int owned_count,
                                   bool is_selected,
                                   bool is_disabled);

} // namespace game::ui
