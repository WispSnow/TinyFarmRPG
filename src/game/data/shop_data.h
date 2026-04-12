#pragma once

#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>

namespace game::data {

struct ShopBuyEntryData {
    std::string item_id_{};
    entt::id_type item_id_hash_{entt::null};
    int buy_price_{0};
    std::optional<int> stock_{};
};

struct ShopSellRuleData {
    std::string item_id_{};
    entt::id_type item_id_hash_{entt::null};
    int sell_price_{0};
};

struct ShopData {
    std::string id_{};
    entt::id_type id_hash_{entt::null};
    std::string title_{};
    std::string greeting_{};
    std::vector<ShopBuyEntryData> buy_entries_{};
};

} // namespace game::data
