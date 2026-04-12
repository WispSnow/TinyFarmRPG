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

enum class ShopMenuFocusArea {
    ModeToggle,
    EntryList,
    Quantity,
    PrimaryAction
};

enum class ShopMenuNavigationInput {
    Up,
    Down,
    Left,
    Right,
    Confirm
};

/// @brief 纯输入导航 helper 的最小状态快照。
struct ShopMenuNavigationState {
    bool is_buy_mode{true};
    ShopMenuFocusArea focus_area{ShopMenuFocusArea::EntryList};
    bool has_buy_entries{false};
    bool has_sell_entries{false};
    bool quantity_adjustable{false};

    [[nodiscard]] bool hasCurrentEntries() const {
        return is_buy_mode ? has_buy_entries : has_sell_entries;
    }
};

/// @brief 纯输入导航 helper 的决策输出。
struct ShopMenuNavigationDecision {
    ShopMenuFocusArea next_focus_area{ShopMenuFocusArea::EntryList};
    bool switch_mode{false};
    bool next_is_buy_mode{true};
    int entry_delta{0};
    int quantity_delta{0};
    bool confirm_trade{false};
};

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

/// @brief 根据当前 mode 是否有条目，选择合适的默认焦点区域。
[[nodiscard]] ShopMenuFocusArea resolvePreferredShopMenuFocus(bool has_current_entries);

/// @brief 根据当前焦点区域与菜单输入计算下一步商店导航决策。
[[nodiscard]] ShopMenuNavigationDecision resolveShopMenuNavigation(const ShopMenuNavigationState& state,
                                                                   ShopMenuNavigationInput input);

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
