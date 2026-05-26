#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <entt/entity/entity.hpp>

namespace game::component {
struct ItemStack;
}

namespace game::data {
class ItemCatalog;
struct ShopBuyEntryData;
struct ShopSellRuleData;
}

namespace game::domain {
enum class ShopTradeFailureReason : std::uint8_t;
}

namespace game::runtime {
class LocalizationService;
}

namespace game::debug {

enum class ShopDebugTradeMode : std::uint8_t {
    Buy = 0,
    Sell
};

struct ShopDebugBuyRow {
    entt::id_type item_id_hash{entt::null};
    std::string item_id{};
    std::string item_name{};
    int owned_count{0};
    int buy_price{0};
};

struct ShopDebugSellRow {
    int slot_index{-1};
    entt::id_type item_id_hash{entt::null};
    std::string item_id{};
    std::string item_name{};
    int stack_count{0};
    int sell_price{0};
    bool is_sellable{false};
};

[[nodiscard]] ShopDebugBuyRow buildShopDebugBuyRow(const game::data::ShopBuyEntryData& buy_entry,
                                                   const game::data::ItemCatalog* item_catalog,
                                                   const game::runtime::LocalizationService* localization,
                                                   int owned_count);
[[nodiscard]] ShopDebugSellRow buildShopDebugSellRow(int slot_index,
                                                     const game::component::ItemStack& stack,
                                                     const game::data::ShopSellRuleData* sell_rule,
                                                     const game::data::ItemCatalog* item_catalog,
                                                     const game::runtime::LocalizationService* localization);
[[nodiscard]] bool isShopDebugSellable(const game::component::ItemStack& stack,
                                       const game::data::ShopSellRuleData* sell_rule);
[[nodiscard]] int clampShopDebugQuantity(int requested_quantity, int max_quantity);
[[nodiscard]] std::string_view shopDebugTradeFailureLabel(ShopDebugTradeMode mode,
                                                          game::domain::ShopTradeFailureReason reason);

} // namespace game::debug
